#include <numeric>
#include <chrono>
#include "Voxel.h"
#include <FVCore.h>

struct TriangleOctree {
    Vector3 aabbCenter;
    float halfExtent;

    std::vector<Triangle> triangles;
    std::vector<TriangleOctree> subdivisions;

    AABB aabb() const {
        auto halfExt = Vector3(halfExtent, halfExtent, halfExtent);
        return { aabbCenter - halfExt, aabbCenter + halfExt };
    }

    void subdivide(int depthLevel) {
        std::vector<Triangle> buffer;
        return subdivide(depthLevel, buffer);
    }

    void subdivide(int depthLevel, std::vector<Triangle>& buffer) {
        if (depthLevel <= 0)
            return;

        buffer.reserve(this->triangles.size());

        auto halfExt = Vector3(halfExtent, halfExtent, halfExtent);
        auto aabbMin = aabbCenter - halfExt;

        subdivisions.reserve(8);

        AABB div[8];
        for (int n = 0; n < 8; ++n) {
            const int x = n & 1;
            const int y = (n >> 1) & 1;
            const int z = (n >> 2) & 1;

            AABB& aabb = div[n];
            aabb.min = aabbMin;
            aabb.max = aabbMin + halfExt;
            aabb.min.x += halfExt.x * x;
            aabb.max.x += halfExt.x * x;
            aabb.min.y += halfExt.y * y;
            aabb.max.y += halfExt.y * y;
            aabb.min.z += halfExt.z * z;
            aabb.max.z += halfExt.z * z;

            buffer.clear();
            for (const Triangle& t : this->triangles) {
                if (aabb.overlapTest(t))
                    buffer.push_back(t);
            }
            if (buffer.empty() == false) {
                TriangleOctree node{ aabb.center(), halfExtent * 0.5f, buffer, {}};
                this->subdivisions.push_back(node);
            }
        }
        buffer.clear();

        //for (auto& sub : subdivisions) {
        //    Log::debug(std::format("AABB min:{} max:{}, triangles:{}", sub.aabb.min, sub.aabb.max, sub.triangles.size()));
        //}

        if (depthLevel > 0) {
            for (auto& sub : subdivisions)
                sub.subdivide(depthLevel - 1, buffer);
        }
    }
};


std::shared_ptr<Voxelizer> voxelize(const std::vector<Triangle>& triangles, int depth) {
    AABB aabb{};
    for (auto& tri : triangles) {
        aabb.expand({ tri.p0, tri.p1, tri.p2 });
    }
    if (aabb.isNull())
        return nullptr;

    Vector3 scale = aabb.extents();
    for (float& s : scale.val) {
        if (s == 0.0f) s = 1.0;
    }

    auto quantize = AffineTransform3::identity.scaled(scale).translated(aabb.min);
    auto normalize = quantize.inverted();

    // normalize triangles
    std::vector<Triangle> normalizedTriangles;
    normalizedTriangles.reserve(triangles.size());
    for (const Triangle& tri : triangles) {
        Triangle normTri = { tri.p0.applying(normalize),
            tri.p1.applying(normalize),
            tri.p2.applying(normalize) };
        normalizedTriangles.push_back(normTri);
    }

    auto start = std::chrono::high_resolution_clock::now();

    TriangleOctree tree {
        Vector3(0.5f, 0.5f, 0.5f), 0.5f,
        normalizedTriangles,
        {}
    };
    tree.subdivide(std::max(depth-1, 0));

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);

    struct LeafNodeCounter {
        const TriangleOctree& tree;
        size_t count() const {
            if (tree.subdivisions.empty())
                return 1;
            return std::reduce(tree.subdivisions.begin(),
                               tree.subdivisions.end(),
                               size_t(0),
                               [](size_t r, const auto& node) {
                return r + LeafNodeCounter{node}.count();
            });
        }
    };

    struct NodeCounter {
        const TriangleOctree& tree;
        size_t count() const {
            return 1 + std::reduce(tree.subdivisions.begin(),
                                   tree.subdivisions.end(),
                                   size_t(0),
                                   [](size_t r, const auto& node) {
                return r + NodeCounter{node}.count();
            });
        }
    };

    Log::info(std::format("triangle-octree(depth:{}) generated with nodes:{}, leaf-nodes:{}, elapsed:{}",
                          depth,
                          NodeCounter{ tree }.count(),
                          LeafNodeCounter{ tree }.count(),
                          elapsed.count()));

    return std::make_shared<Voxelizer>();
}
