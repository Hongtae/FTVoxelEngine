#include <numeric>
#include <chrono>
#include "Voxel.h"
#include <FVCore.h>

struct TriangleOctree {
    AABB aabb;
    std::vector<Triangle> triangles;
    std::vector<TriangleOctree> subdivisions;

    void subdivide(int depthLevel) {
        std::vector<Triangle> buffer;
        return subdivide(depthLevel, buffer);
    }

    void subdivide(int depthLevel, std::vector<Triangle>& buffer) {
        if (depthLevel <= 0)
            return;

        buffer.reserve(this->triangles.size());

        auto extent = aabb.extents();
        auto halfExt = extent * 0.5f;
        subdivisions.reserve(8);

        AABB div[8];
        for (int n = 0; n < 8; ++n) {
            const int x = n & 1;
            const int y = (n >> 1) & 1;
            const int z = (n >> 2) & 1;

            AABB& aabb = div[n];
            aabb.min = this->aabb.min;
            aabb.max = aabb.min + halfExt;
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
                TriangleOctree node{ aabb, buffer, {} };
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

    Matrix4 quantize = AffineTransform3::identity.scaled(scale).matrix4();
    Matrix4 normalize = quantize.inverted();

    auto start = std::chrono::high_resolution_clock::now();

    TriangleOctree tree {
        aabb,
        triangles,
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

    Log::info(std::format("triangle-octree(depth:{}) generated with leaf-nodes:{}, elapsed:{}",
                          depth, LeafNodeCounter{ tree }.count(),
                          elapsed.count()));

    return std::make_shared<Voxelizer>();
}
