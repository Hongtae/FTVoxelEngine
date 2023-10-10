#include <numeric>
#include <chrono>
#include <limits>
#include "Voxel.h"
#include <FVCore.h>

struct TriangleOctree {
    TriangleQuery triangleDataSource;

    Vector3 aabbCenter;
    uint32_t halfExtentExponent;

    std::vector<uint64_t> triangles;
    std::vector<TriangleOctree> subdivisions;

    AABB aabb() const {
        float halfExtent = 1.0f;
        for (int i = 0; i < halfExtentExponent; ++i)
            halfExtent = halfExtent * 0.5f;

        auto halfExt = Vector3(halfExtent, halfExtent, halfExtent);
        return { aabbCenter - halfExt, aabbCenter + halfExt };
    }

    void subdivide(int depthLevel) {
        std::vector<uint64_t> buffer;
        return subdivide(depthLevel, buffer);
    }

    void subdivide(int depthLevel, std::vector<uint64_t>& buffer) {
        if (depthLevel <= 0)
            return;

        buffer.reserve(this->triangles.size());

        float halfExtent = 1.0f;
        for (int i = 0; i < halfExtentExponent; ++i)
            halfExtent = halfExtent * 0.5f;

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
            for (auto t : this->triangles) {
                if (aabb.overlapTest(triangleDataSource(t)))
                    buffer.push_back(t);
            }
            if (buffer.empty() == false) {
                TriangleOctree node{ triangleDataSource, aabb.center(), halfExtentExponent + 1, buffer, {}};
                this->subdivisions.push_back(node);
            }
        }
        buffer.clear();

        if (depthLevel > 0) {
            for (auto& sub : subdivisions)
                sub.subdivide(depthLevel - 1, buffer);
        }
    }

    size_t numberOfDescendants() const {
        return std::reduce(subdivisions.begin(), subdivisions.end(),
                           size_t(1),
                           [](size_t r, const auto& node) {
                               return r + node.numberOfDescendants();
                           });
    }

    size_t numberOfLeafNodes() const {
        if (subdivisions.empty()) return 1;
        return std::reduce(subdivisions.begin(), subdivisions.end(),
                           size_t(0),
                           [](size_t r, const auto& node) {
                               return r + node.numberOfLeafNodes();
                           });
    }

    std::vector<AABBOctree::Node> makeAABBOctreeVector(PayloadQuery payload) const {
        std::vector<AABBOctree::Node> nodes(1);
        AABBOctree::Node& node = nodes.front();
        constexpr float q = float(std::numeric_limits<uint16_t>::max());
        node.aabbCenter[0] = static_cast<uint16_t>(this->aabbCenter.val[0] * q);
        node.aabbCenter[1] = static_cast<uint16_t>(this->aabbCenter.val[1] * q);
        node.aabbCenter[2] = static_cast<uint16_t>(this->aabbCenter.val[2] * q);
        node.aabbHalfExtentExponent = this->halfExtentExponent;
        node.flags = 0;
        if (this->subdivisions.empty()) {
            FVASSERT(this->triangles.empty() == false);
            node.flags |= AABBOctree::FlagPayload;
            node.payload = payload(this->triangles.front());
        } else {
            for (auto& child : this->subdivisions) {
                auto descendants = child.makeAABBOctreeVector(payload);
                nodes.insert(nodes.end(), descendants.begin(), descendants.end());
            }
            node.strideToNextSibling = nodes.size() - 1;
        }
        return nodes;
    }
};


std::shared_ptr<AABBOctree> voxelize(uint64_t numTriangles, int depth, TriangleQuery triangleQuery, PayloadQuery payloadQuery) {
    std::vector<Triangle> triangles;
    triangles.reserve(numTriangles);

    AABB aabb{};
    for (uint64_t i = 0; i < numTriangles; ++i) {
        const auto& tri = triangleQuery(i);
        triangles.push_back(tri);
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
    for (auto& tri : triangles) {
        tri.p0.apply(normalize);
        tri.p1.apply(normalize);
        tri.p2.apply(normalize);
    }

    std::vector<uint64_t> triangleIndices(triangles.size());
    std::iota(triangleIndices.begin(), triangleIndices.end(), 0); // fill with triangle indices

    auto start = std::chrono::high_resolution_clock::now();

    TriangleOctree tree {
        [&](uint64_t index)-> const Triangle& { return triangles.at(index); },
        Vector3(0.5f, 0.5f, 0.5f), 1,
        triangleIndices,
        {}
    };
    tree.subdivide(std::max(depth-1, 0));

    auto aabbOctree = std::make_shared<AABBOctree>();
    aabbOctree->aabb = aabb;
    aabbOctree->subdivisions = tree.makeAABBOctreeVector(payloadQuery);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start);

    Log::info(std::format("triangle-octree(depth:{}) generated with nodes:{}, leaf-nodes:{}, elapsed:{}",
                          depth,
                          tree.numberOfDescendants(), tree.numberOfLeafNodes(),
                          elapsed.count()));

    return aabbOctree;
}
