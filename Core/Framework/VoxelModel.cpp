#include "AffineTransform3.h"
#include "VoxelModel.h"

using namespace FV;

bool VoxelOctree::mergeSolidBranches() {
    int n = 0;
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    uint16_t a = 0;
    uint16_t metallic = 0;
    uint16_t roughness = 0;

    for (auto p : this->subdivisions) {
        if (p) {
            n++;
            r += p->value.color.r;
            g += p->value.color.g;
            b += p->value.color.b;
            a += p->value.color.a;
            metallic += p->value.metallic;
            roughness += p->value.roughness;
        }
    }
    if (n > 0) {
        this->value.color.r = r / n;
        this->value.color.g = g / n;
        this->value.color.b = b / n;
        this->value.color.a = a / n;
        this->value.metallic = metallic / n;
        this->value.roughness = roughness / n;

        if (n == 8) {
            bool combinable = true;
            for (auto p : this->subdivisions) {
                if (p->isLeafNode() == false ||
                    p->value != this->value) {
                    combinable = false;
                    break;
                }
            }
            if (combinable) {
                for (int i = 0; i < 8; ++i) {
                    auto p = this->subdivisions[i];
                    FVASSERT_DEBUG(p);
                    FVASSERT_DEBUG(p->isLeafNode());
                    delete p;
                    this->subdivisions[i] = nullptr;
                }
            }
        }
        return true;
    }
    return false;
}

VolumeArray VoxelOctree::makeArray(const AABB& aabb, uint32_t maxDepth) const {
    if (aabb.isNull())
        return {};

    struct MakeArray {
        const VoxelOctree* node;
        const Vector3 center;
        uint32_t depth;
        void operator() (std::vector<VolumeArray::Node>& vector, uint32_t maxDepth) const {
            auto index = vector.size();
            vector.push_back({});
            auto& n = vector.at(index);
            constexpr float q = float(std::numeric_limits<uint16_t>::max());
            n.x = static_cast<uint16_t>(center.x * q);
            n.y = static_cast<uint16_t>(center.y * q);
            n.z = static_cast<uint16_t>(center.z * q);
            n.depth = depth;
            n.flags = 0;
            n.color = node->value.color;

            if (depth < maxDepth) {
                uint32_t exp = (126U - depth) << 23;
                float halfExtent = std::bit_cast<float>(exp);

                for (int i = 0; i < 8; ++i) {
                    auto p = node->subdivisions[i];
                    if (p) {
                        const int x = i & 1;
                        const int y = (i >> 1) & 1;
                        const int z = (i >> 2) & 1;

                        Vector3 pt = {
                             center.x + halfExtent * (float(x) - 0.5f),
                             center.y + halfExtent * (float(y) - 0.5f),
                             center.z + halfExtent * (float(z) - 0.5f),
                        };
                        MakeArray{ p, pt, depth + 1 }(vector, maxDepth);
                    }
                }
            }
            auto advance = (vector.size() - index) + 1;
            FVASSERT_DEBUG(advance < std::numeric_limits<decltype(n.advance)>::max());
            n.advance = static_cast<decltype(n.advance)>(advance);
            if (n.advance == 1) { // leaf-node
                n.flags |= VolumeArray::FlagLeafNode;
                n.flags |= VolumeArray::FlagMaterial;
            }
        }
    };
    VolumeArray volumes = {};
    volumes.aabb = aabb;
    volumes.data.reserve(numDescendants());
    maxDepth = std::clamp(maxDepth, 0U, VoxelOctree::maxDepth);
    MakeArray{ this, Vector3(0.5f, 0.5f, 0.5f), 0 }(volumes.data, maxDepth);
    volumes.data.shrink_to_fit();
    return volumes;
}

VoxelModel::VoxelModel(const AABB& volume, int depth)
    : _root(nullptr)
    , aabb(volume)
    , maxDepth(std::max(depth, 0)) {
}

VoxelModel::VoxelModel(VoxelOctreeBuilder* builder, int depth)
    : _root(nullptr)
    , maxDepth(std::max(depth, 0)) {
}

VoxelModel::~VoxelModel() {
    if (_root)
        deleteNode(_root);
}

void VoxelModel::update(uint32_t x, uint32_t y, uint32_t z, const Voxel& value) {
    const auto res = resolution();
    if (x >= res || y >= res || z >= res) {
        throw std::out_of_range("Invalid index");
    }
    struct Update {
        uint32_t dim;
        VoxelOctree* node;
        const Voxel& value;
        bool operator() (uint32_t x, uint32_t y, uint32_t z) {
            FVASSERT_DEBUG(dim > 0);

            auto nx = x / dim;
            auto ny = y / dim;
            auto nz = z / dim;
            uint32_t index = ((nz & 1) << 2) | ((ny & 1) << 1) | (nx & 1);
            auto p = node->subdivisions[index];
            bool updated = false;
            if (p == nullptr) {
                p = new VoxelOctree(value);
                node->subdivisions[index] = p;
                updated = true;
            }
            if (dim > 1) {
                if (Update{ dim >> 1, p, value }(x % dim, y % dim, z % dim)) {
                    updated = true;
                }
            } else {
                if (p->value != value) {
                    p->value = value;
                    updated = true;
                }
            }
            if (updated) {
                node->mergeSolidBranches();
            }
            return updated;
        }
    };
    if (aabb.isNull()) {
        throw std::runtime_error("Invalid object (AABB is null)");
    }
    if (_root == nullptr) {
        _root = new VoxelOctree(value);
    }
    if (res > 1) {
        if (Update{ res >> 1, _root, value }(x, y, z))
            _root->mergeSolidBranches();
    } else {
        FVASSERT_DEBUG(_root->isLeafNode());
        _root->value = value;
    }
}

void VoxelModel::erase(uint32_t x, uint32_t y, uint32_t z) {
    const auto res = resolution();
    if (x >= res || y >= res || z >= res) {
        throw std::out_of_range("Invalid index");
    }
    if (_root) {
        struct EraseNode {
            uint32_t dim;
            VoxelOctree* node;
            bool operator() (uint32_t x, uint32_t y, uint32_t z) {
                FVASSERT_DEBUG(dim > 0);

                auto nx = x / dim;
                auto ny = y / dim;
                auto nz = z / dim;
                uint32_t index = ((nz & 1) << 2) | ((ny & 1) << 1) | (nx & 1);

                if (dim > 1) {
                    if (node->isLeafNode()) {
                        for (int i = 0; i < 8; ++i) {
                            node->subdivisions[i] = new VoxelOctree({ node->value });
                        }
                    }
                    auto p = node->subdivisions[index];
                    if (p) {
                        if (EraseNode{ dim >> 1, p }(x % dim, y % dim, z % dim)) {
                            if (p->isLeafNode()) {
                                deleteNode(p);
                                node->subdivisions[index] = nullptr;
                            }
                            node->mergeSolidBranches();
                            return true;
                        }
                    }
                } else {
                    auto p = node->subdivisions[index];
                    if (p) {
                        FVASSERT_DEBUG(p->isLeafNode());
                        deleteNode(p);
                        node->subdivisions[index] = nullptr;
                        node->mergeSolidBranches();
                        return true;
                    } else {
                        if (node->isLeafNode()) {
                            for (int i = 0; i < 8; ++i) {
                                if (i != index)
                                    node->subdivisions[i] = new VoxelOctree({ node->value });
                            }
                            return true;
                        }
                    }
                    return false;
                }
                return false;
            }
        };
        if (res > 1) {
            if (EraseNode{ res >> 1, _root, }(x, y, z)) {
                if (_root->isLeafNode()) {
                    deleteNode(_root);
                    _root = nullptr;
                } else {
                    _root->mergeSolidBranches();
                }
            }
        } else {
            FVASSERT_DEBUG(_root->isLeafNode());
            deleteNode(_root);
            _root = nullptr;
        }
    }
}

std::optional<Voxel> VoxelModel::lookup(uint32_t x, uint32_t y, uint32_t z) const {
    const auto res = resolution();
    if (x >= res || y >= res || z >= res) {
        throw std::out_of_range("Invalid index");
    }
    struct Lookup {
        size_t dim;
        const VoxelOctree* node;
        const VoxelOctree* operator() (uint32_t x, uint32_t y, uint32_t z) {
            FVASSERT_DEBUG(dim > 0);

            auto nx = x / dim;
            auto ny = y / dim;
            auto nz = z / dim;
            uint32_t index = ((nz & 1) << 2) | ((ny & 1) << 1) | (nx & 1);
            auto p = node->subdivisions[index];
            if (p)
                return Lookup{ dim >> 1, p }(x % dim, y % dim, z % dim);
            return node;
        }
    };
    if (aabb.isNull())
        return {};
    if (_root) {
        if (res > 1) {
            if (auto p = Lookup{ res >> 1, _root }(x, y, z); p->isLeafNode())
                return p->value;
        } else {
            FVASSERT_DEBUG(_root->isLeafNode());
            return _root->value;
        }
    }
    return {};
}

void VoxelModel::optimize() {
    struct ForEachNode {
        VoxelOctree* node;
        void operator() (std::function<void(VoxelOctree*)>& callback) {
            for (auto p : node->subdivisions) {
                if (p)
                    ForEachNode{ p }(callback);
            }
            callback(node);
        }
    };
    std::function<void(VoxelOctree*)> fn = [&](VoxelOctree* node) {
        node->mergeSolidBranches();
    };
    ForEachNode{ _root }(fn);
}

int VoxelModel::enumerateLevel(int depth, std::function<void(const AABB&, const VoxelOctree&)> cb) const {
    using Callback = std::function<void(const AABB&, const VoxelOctree&)>;

    struct IterateDepth {
        const VoxelOctree& node;
        const Vector3 center;
        uint32_t level;
        Callback& callback;
        uint32_t operator() (uint32_t depth, Callback& cb) {
            uint32_t exp = (126U - depth) << 23;
            float halfExtent = std::bit_cast<float>(exp);

            if (level < depth) {
                uint32_t result = 0;
                for (int i = 0; i < 8; ++i) {
                    auto p = node.subdivisions[i];
                    if (p == nullptr) continue;

                    const int x = i & 1;
                    const int y = (i >> 1) & 1;
                    const int z = (i >> 2) & 1;

                    Vector3 pt = {
                         center.x + halfExtent * (float(x) - 0.5f),
                         center.y + halfExtent * (float(y) - 0.5f),
                         center.z + halfExtent * (float(z) - 0.5f),
                    };
                    result += IterateDepth{ *p, pt, level + 1, cb }(depth, cb);
                }
                return result;
            }
            AABB aabb = {
                center - Vector3(halfExtent, halfExtent, halfExtent),
                center + Vector3(halfExtent, halfExtent, halfExtent)
            };
            cb(aabb, node);
            return 1;
        }
    };
    if (_root) {
        IterateDepth{ *_root, aabb.center(), 0, cb }(std::clamp(depth, 0, 125), cb);
    }
    return 0;
}

std::optional<VoxelModel::RayHitResult> VoxelModel::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                if (rayHit.has_value()) {
                    auto p1 = rayHit.value();
                    if (p2.t < p1.t) {
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
                    if (p2.t > p1.t) {
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

uint64_t VoxelModel::rayTest(const Vector3& rayOrigin, const Vector3& dir,
                             std::function<bool(const RayHitResult&)> filter) const {
    if (aabb.isNull())
        return 0;

    Vector3 origin = aabb.min;
    Vector3 scale = aabb.extents();
    FVASSERT_DEBUG(scale.x * scale.y * scale.z != 0.0f);

    auto quantize = AffineTransform3::identity.scaled(scale).translated(origin);
    auto normalize = quantize.inverted();

    auto rayStart = rayOrigin.applying(normalize);
    auto rayDir = dir.applying(normalize.matrix3);

    struct RayTestNode {
        const VoxelOctree* node;
        const Vector3 center;
        const uint32_t depth;
        uint32_t resolution;
        bool& continueRayTest;
        std::function<bool(const RayHitResult&)> callback;
        uint64_t rayTest(const Vector3& start, const Vector3& dir) const {
            uint32_t exp = (126 - depth) << 23;
            float halfExtent = std::bit_cast<float>(exp);
            AABB aabb = {
                center - Vector3(halfExtent, halfExtent, halfExtent),
                center + Vector3(halfExtent, halfExtent, halfExtent)
            };
            auto r = aabb.rayTest(start, dir);
            if (r >= 0.0f) {
                uint64_t numHits = 0;
                bool leafNode = true;
                for (int i = 0; i < 8; ++i) {
                    if (continueRayTest == false)
                        break;
                    auto p = node->subdivisions[i];
                    if (p) {
                        leafNode = false;

                        const int x = i & 1;
                        const int y = (i >> 1) & 1;
                        const int z = (i >> 2) & 1;

                        Vector3 pt = {
                             center.x + halfExtent * (float(x) - 0.5f),
                             center.y + halfExtent * (float(y) - 0.5f),
                             center.z + halfExtent * (float(z) - 0.5f),
                        };
                        numHits += RayTestNode{ p, pt, depth + 1, resolution, continueRayTest, callback }
                        .rayTest(start, dir);
                    }
                }
                if (leafNode) {
                    uint32_t x = std::floor(center.x * resolution);
                    uint32_t y = std::floor(center.y * resolution);
                    uint32_t z = std::floor(center.z * resolution);
                    if (callback(RayHitResult{ r, node, {x, y, z}, depth }) == false) {
                        continueRayTest = false;
                    }
                    return 1;
                }
                return numHits;
            }
            return 0;
        }
    };

    FVASSERT_DEBUG(_root);

    bool continueRayTest = true;
    std::function<bool(const RayHitResult&)> callback;
    if (filter) {
        callback = [&](const RayHitResult& r) {
            Vector3 hit = rayStart + rayDir * r.t;
            hit.apply(quantize);
            return filter({ (hit - rayOrigin).magnitude(), r.node });
        };
    } else {
        callback = [](auto&&) { return true; };
    }

    return RayTestNode{
        _root, Vector3(0.5f, 0.5f, 0.5f), 0, resolution(), continueRayTest, callback 
    }.rayTest(rayStart, rayDir);
}

void VoxelModel::deleteNode(VoxelOctree* node) {
    ForEachNode{ node }.backward(
        [](VoxelOctree* node) {
            delete node;
        });
}
