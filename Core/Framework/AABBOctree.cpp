#include <limits>
#include <numeric>
#include "AABBOctree.h"
#include "Matrix4.h"
#include "AffineTransform3.h"

using namespace FV;

std::optional<AABBOctreeLayer::RayHitResult>
AABBOctreeLayer::rayTest(const Vector3& rayOrigin, const Vector3& dir,
                         RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(
            rayOrigin, dir,
            [&](const auto& p2) {
                if (rayHit.has_value()) {
                    auto p1 = rayHit.value();
                    auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                    auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                    if (sq2 < sq1) {
                        rayHit = p2;
                    }
                } else {
                    rayHit = p2;
                }
                return true;
            });
    } else if (option == LongestHit) {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                if (rayHit.has_value()) {
                    auto p1 = rayHit.value();
                    auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                    auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                    if (sq2 > sq1) {
                        rayHit = p2;
                    }
                } else {
                    rayHit = p2;
                }
                return true;
            });
    } else {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                rayHit = p2;
                return false;
            });
    }
    return rayHit;
}

uint32_t AABBOctreeLayer::rayTest(const Vector3& rayOrigin,
                                  const Vector3& dir,
                                  std::function<bool(const RayHitResult&)> filter) const {
    if (aabb.isNull()) return 0;

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
    for (float& s : scale.val) {
        if (s == 0.0f) s = 1.0;
    }

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3); // linear only

    uint32_t numHits = 0;
    constexpr float q = 1.0f / float(std::numeric_limits<uint16_t>::max());

    uint64_t index = 0;
    while (index < data.size()) {
        const auto& node = data.at(index);
        Vector3 center = Vector3(float(node.center[0]),
                                 float(node.center[1]),
                                 float(node.center[2])) * q;
        float halfExtent = 0.5f;
        for (int i = 0; i < node.depth; ++i)
            halfExtent = halfExtent * 0.5f;

        AABB aabb = {
            center - Vector3(halfExtent, halfExtent, halfExtent),
            center + Vector3(halfExtent, halfExtent, halfExtent)
        };

        auto r = aabb.rayTest(rayStart, rayDir);
        if (r >= 0.0f) {
            if (node.isLeaf()) {
                numHits++;
                Vector3 hitPoint = (rayStart + rayDir * r).applying(quantize);
                if (filter(RayHitResult{ hitPoint, node.payload }) == false)
                    break;
            }
            index++;
        } else {
            if (node.isLeaf())
                index++;
            else
                index += node.strideToNextSibling;
        }
    }
    return numHits;
}

size_t AABBOctree::_numberOfDescendants() const {
    struct Counter {
        const Node& node;
        size_t operator() () const {
            return std::reduce(node.subdivisions.begin(),
                               node.subdivisions.end(),
                               size_t(1),
                               [](size_t r, const Node& node) {
                                   return r + Counter{ node }();
                               });
        }
    };
    return Counter{ root }();
}

size_t AABBOctree::_numberOfLeafNodes() const {
    struct Counter {
        const Node& node;
        size_t operator() () const {
            if (node.subdivisions.empty()) return 1;
            return std::reduce(node.subdivisions.begin(),
                               node.subdivisions.end(),
                               size_t(0),
                               [](size_t r, const Node& node) {
                                   return r + Counter{ node }();
                               });
        }
    };
    return Counter{ root }();
}

std::shared_ptr<AABBOctree>
AABBOctree::makeTree(uint32_t maxDepth,
                     uint64_t numTriangles,
                     uint64_t baseIndex,
                     TriangleQuery triangleQuery,
                     PayloadQuery payloadQuery) {
    std::vector<Triangle> triangles;
    triangles.reserve(numTriangles);

    AABB aabb{};
    for (uint64_t i = 0; i < numTriangles; ++i) {
        const auto& tri = triangleQuery(i + baseIndex);
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
    std::iota(triangleIndices.begin(), triangleIndices.end(), baseIndex); // fill with triangle indices

    struct Counter {
        uint64_t numNodes;
        uint64_t numLeafNodes;
    };

    struct Subdivider {
        Node& node;
        uint32_t maxDepth;
        std::vector<uint64_t> triangles; // overlapped-triangles
        TriangleQuery& triangleQuery;
        PayloadQuery& payloadQuery;
        void operator() (int depthLevel, std::vector<uint64_t>& buffer, Counter& counter) {
            if (depthLevel <= 0) return;

            float halfExtent = 0.5f;
            for (uint32_t i = 0; i < node.depth; ++i)
                halfExtent *= 0.5f;

            auto pivot = node.center - Vector3(halfExtent, halfExtent, halfExtent) * 0.5f;

            node.subdivisions.reserve(8);
            buffer.reserve(triangles.size());

            for (int n = 0; n < 8; ++n) {
                const int x = n & 1;
                const int y = (n >> 1) & 1;
                const int z = (n >> 2) & 1;

                Vector3 aabbCenter = pivot;
                aabbCenter.x += halfExtent * x;
                aabbCenter.y += halfExtent * y;
                aabbCenter.z += halfExtent * z;

                Node child = { aabbCenter, node.depth + 1, 0, {} };
                AABB aabb = child.aabb();

                buffer.clear();
                for (auto t : this->triangles) {
                    if (aabb.overlapTest(triangleQuery(t)))
                        buffer.push_back(t);
                }
                if (buffer.empty() == false) {
                    child.payload = payloadQuery(buffer.data(), buffer.size(), child.center);
                    if (depthLevel > 1) {
                        Subdivider div{ child, maxDepth, buffer, triangleQuery, payloadQuery };
                        div(depthLevel - 1, buffer, counter);
                    } else {
                        counter.numLeafNodes++;
                    }
                    node.subdivisions.push_back(child);
                    counter.numNodes++;
                }
            }
            node.subdivisions.shrink_to_fit();
        }
    };

    TriangleQuery normalizedTriangleQuery = [&](uint64_t index) -> const Triangle& {
        FVASSERT_DEBUG(index >= baseIndex);
        return triangles.at(index - baseIndex);
    };
    PayloadQuery quantizedTrianglePayloadQuery = [&](uint64_t* indices, size_t size, const Vector3& position) -> AABBOctree::Payload {
        return payloadQuery(indices, size, position.applying(quantize));
    };

    Node node = { Vector3(0.5f, 0.5f, 0.5f), 0, 0, {} };
    node.payload = payloadQuery(triangleIndices.data(), triangleIndices.size(),
                                node.center.applying(quantize));

    auto sub = Subdivider{ node, maxDepth, std::move(triangleIndices),
        normalizedTriangleQuery, quantizedTrianglePayloadQuery };

    std::vector<uint64_t> buffer;
    Counter counter{ 0, 0 };
    sub(maxDepth, buffer, counter);
    if (counter.numLeafNodes == 0)
        counter.numLeafNodes = 1; // root
    counter.numNodes += 1; // root

    auto octrees = std::make_shared<AABBOctree>();
    octrees->root = std::move(node);
    octrees->aabb = aabb;
    octrees->maxDepth = maxDepth;
    octrees->numDescendants = counter.numNodes;
    octrees->numLeafNodes = counter.numLeafNodes;

    return octrees;
}

std::shared_ptr<AABBOctreeLayer> AABBOctree::makeLayer(uint32_t maxDepth) const {
    struct MakeLayerNodeArray {
        const Node& node;
        uint32_t maxDepth;
        void operator() (std::vector<AABBOctreeLayer::Node>& nodes) const {
            auto index = nodes.size();
            nodes.push_back({});
            auto& n = nodes.at(index);
            constexpr float q = float(std::numeric_limits<uint16_t>::max());
            n.center[0] = static_cast<uint16_t>(node.center.x * q);
            n.center[1] = static_cast<uint16_t>(node.center.y * q);
            n.center[2] = static_cast<uint16_t>(node.center.z * q);
            n.depth = node.depth;
            n.flags = 0;
            if (node.subdivisions.empty() || node.depth >= maxDepth) {
                n.flags |= AABBOctreeLayer::FlagPayload;
                n.payload = node.payload;
            } else {
                for (auto& sub : node.subdivisions) {
                    MakeLayerNodeArray{ sub, maxDepth }(nodes);
                }
                auto stride = nodes.size() - index;
                constexpr auto maxLength = std::numeric_limits<AABBOctreeLayer::Index>::max();
                FVASSERT_DEBUG(uint64_t(stride) < uint64_t(maxLength));
                n.strideToNextSibling = AABBOctreeLayer::Index(stride);
            }
        }
    };
    auto layer = std::make_shared<AABBOctreeLayer>();
    layer->aabb = this->aabb;
    layer->data.reserve(this->numDescendants);
    MakeLayerNodeArray{ root, maxDepth }(layer->data);
    layer->data.shrink_to_fit();
    return layer;
}

std::optional<AABBOctree::RayHitResult>
AABBOctree::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                if (sq2 < sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else if (option == LongestHit) {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            if (rayHit.has_value()) {
                auto p1 = rayHit.value();
                auto sq1 = (p1.hitPoint - rayOrigin).magnitudeSquared();
                auto sq2 = (p2.hitPoint - rayOrigin).magnitudeSquared();
                if (sq2 > sq1) {
                    rayHit = p2;
                }
            } else {
                rayHit = p2;
            }
            return true;
        });
    } else {
        auto numHits = rayTest(rayOrigin, dir, [&](const auto& p2) {
            rayHit = p2;
            return false;
        });
    }
    return rayHit;
}

uint64_t AABBOctree::rayTest(const Vector3& rayOrigin,
                             const Vector3& dir,
                             std::function<bool(const RayHitResult&)> filter) const {
    if (aabb.isNull()) return 0;

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
    for (float& s : scale.val) {
        if (s == 0.0f) s = 1.0;
    }

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3);

    bool continueRayTest = true;

    struct RayTestNode {
        const Node& node;
        bool& continueRayTest;
        const AffineTransform3& quantize;
        std::function<bool(const RayHitResult&)> filter;
        uint64_t rayTest(const Vector3& start, const Vector3& dir) const {
            auto r = node.aabb().rayTest(start, dir);
            if (r >= 0.0f) {
                if (node.subdivisions.empty()) {
                    Vector3 hitPoint = (start + dir * r).applying(quantize);
                    if (filter(RayHitResult{ hitPoint, node.payload }) == false) {
                        continueRayTest = false;
                    }
                    return 1;
                } else {
                    uint64_t numHits = 0;
                    for (const Node& n : node.subdivisions) {
                        if (!continueRayTest) {
                            break;
                        }
                        numHits += RayTestNode{n, continueRayTest, quantize, filter}.rayTest(start, dir);
                    }
                    return numHits;
                }
            }
            return 0;
        }
    };
    return RayTestNode{ root, continueRayTest, quantize, filter }.rayTest(rayStart, rayDir);
}
