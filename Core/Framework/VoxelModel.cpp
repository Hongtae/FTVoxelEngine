#include "AffineTransform3.h"
#include "VoxelModel.h"
#include "DispatchQueue.h"
#include "Logger.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();

constexpr bool mergeSolidNodes = true;

bool VoxelOctree::mergeSolidBranches() {
    int n = 0;
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    uint16_t a = 0;
    uint32_t metallic = 0;
    uint32_t roughness = 0;

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

        if constexpr (mergeSolidNodes) {
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
            auto advance = (vector.size() - index);
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

VoxelModel::VoxelModel(const AABB& aabb, int depth)
    : _root(nullptr)
    , _scale(0.0f)
    , _center(Vector3::zero)
    , _maxDepth(std::max(depth, 0)) {
    if (aabb.isNull() == false) {
        auto extents = aabb.extents();
        _center = aabb.center();
        _scale = std::max({ extents.x, extents.y, extents.z });
    }
}

VoxelModel::VoxelModel(VoxelOctreeBuilder* builder, int depth)
    : _root(nullptr)
    , _scale(0.0f)
    , _center(Vector3::zero)
    , _maxDepth(std::max(depth, 0)) {
    if (builder) {
        AABB aabb = builder->aabb();
        if (aabb.isNull() == false) {
            auto extents = aabb.extents();
            _center = aabb.center();
            _scale = std::max({ extents.x, extents.y, extents.z });

            if (_scale > epsilon) {

                struct Subdivide {
                    VoxelOctreeBuilder* builder;
                    const Matrix4& transform; // transform to original volume scale
                    Vector3 center; // normalized
                    uint32_t depth;
                    VoxelOctree* node;
                    void operator() (uint32_t maxDepth) const {
                        if (depth < maxDepth) {

                            float halfExtent = VoxelOctree::halfExtent(depth);
                            Vector3 HalfHalfExt = Vector3(halfExtent, halfExtent, halfExtent) * 0.5f;

                            for (int i = 0; i < 8; ++i) {
                                const int x = i & 1;
                                const int y = (i >> 1) & 1;
                                const int z = (i >> 2) & 1;

                                Vector3 pt = {
                                     center.x + halfExtent * (float(x) - 0.5f),
                                     center.y + halfExtent * (float(y) - 0.5f),
                                     center.z + halfExtent * (float(z) - 0.5f),
                                };
                                AABB aabb = { pt - HalfHalfExt, pt + HalfHalfExt };
                                VoxelOctree* sub = new VoxelOctree();
                                if (builder->volumeTest(aabb.applying(transform), sub, node)) {
                                    node->subdivisions[i] = sub;

                                    Subdivide{
                                        builder, transform,
                                        pt, depth + 1, sub
                                    }(maxDepth);
                                } else {
                                    delete sub;
                                }
                            }
                            node->mergeSolidBranches();

                        } else { // leaf-node
                            float halfExtent = VoxelOctree::halfExtent(depth);
                            AABB aabb = {
                                center - Vector3(halfExtent, halfExtent, halfExtent),
                                center + Vector3(halfExtent, halfExtent, halfExtent)
                            };
                            node->value = builder->value(aabb.applying(transform), node);
                        }
                        builder->clear(node);
                    }
                };

                AABB aabb = {
                    _center - Vector3(_scale, _scale, _scale) * 0.5f,
                    _center + Vector3(_scale, _scale, _scale) * 0.5f
                };
                auto p = new VoxelOctree();
                if (builder->volumeTest(aabb, p, nullptr)) {
                    AffineTransform3 transform = AffineTransform3::identity
                        .scaled(aabb.extents())
                        .translated(aabb.min);
                    Subdivide{
                        builder, transform.matrix4(),
                        Vector3(0.5f, 0.5f, 0.5f), 0, p}(_maxDepth);
                } else {
                    deleteNode(p);
                    p = nullptr;
                }
                _root = p;
            } else {
                _scale = 0.0f;
            }
        }
    }
}

VoxelModel::VoxelModel(VoxelOctreeBuilder* builder, int depth, DispatchQueue& queue)
    : _root(nullptr)
    , _scale(0.0f)
    , _center(Vector3::zero)
    , _maxDepth(std::max(depth, 0)) {
    if (builder) {
        AABB aabb = builder->aabb();
        if (aabb.isNull() == false) {
            auto extents = aabb.extents();
            _center = aabb.center();
            _scale = std::max({ extents.x, extents.y, extents.z });

            if (_scale > epsilon) {

                struct SubdivideAsync {
                    DispatchQueue& queue;
                    VoxelOctreeBuilder* builder;
                    const Matrix4& transform; // transform to original volume scale
                    Vector3 center; // normalized
                    uint32_t depth;
                    VoxelOctree* node;
                    void operator() (uint32_t maxDepth) const {
                        if (depth < maxDepth) {

                            float halfExtent = VoxelOctree::halfExtent(depth);
                            Vector3 HalfHalfExt = Vector3(halfExtent, halfExtent, halfExtent) * 0.5f;
                            std::vector<std::shared_ptr<AsyncTask>> tasks;
                            tasks.reserve(8);
                            for (int i = 0; i < 8; ++i) {
                                const int x = i & 1;
                                const int y = (i >> 1) & 1;
                                const int z = (i >> 2) & 1;

                                Vector3 pt = {
                                     center.x + halfExtent * (float(x) - 0.5f),
                                     center.y + halfExtent * (float(y) - 0.5f),
                                     center.z + halfExtent * (float(z) - 0.5f),
                                };
                                AABB aabb = { pt - HalfHalfExt, pt + HalfHalfExt };
                                VoxelOctree* sub = new VoxelOctree();
                                if (builder->volumeTest(aabb.applying(transform), sub, node)) {
                                    node->subdivisions[i] = sub;

                                    SubdivideAsync div{
                                        queue,
                                        builder, transform,
                                        pt, depth + 1, sub
                                    };
                                    if (depth == 1) {
                                        auto task = queue.async(
                                            [div, maxDepth] {
                                                div(maxDepth);
                                            });
                                        tasks.push_back(task);
                                    } else {
                                        div(maxDepth);
                                    }
                                } else {
                                    delete sub;
                                }
                            }
                            AsyncTask::waitAll(std::move(tasks));
                            node->mergeSolidBranches();

                        } else { // leaf-node
                            float halfExtent = VoxelOctree::halfExtent(depth);
                            AABB aabb = {
                                center - Vector3(halfExtent, halfExtent, halfExtent),
                                center + Vector3(halfExtent, halfExtent, halfExtent)
                            };
                            node->value = builder->value(aabb.applying(transform), node);
                        }
                        builder->clear(node);
                    }
                };

                AABB aabb = {
                    _center - Vector3(_scale, _scale, _scale) * 0.5f,
                    _center + Vector3(_scale, _scale, _scale) * 0.5f
                };
                auto p = new VoxelOctree();
                if (builder->volumeTest(aabb, p, nullptr)) {
                    AffineTransform3 transform = AffineTransform3::identity
                        .scaled(aabb.extents())
                        .translated(aabb.min);
                    SubdivideAsync{
                        queue,
                        builder, transform.matrix4(),
                        Vector3(0.5f, 0.5f, 0.5f), 0, p }(_maxDepth);
                } else {
                    deleteNode(p);
                    p = nullptr;
                }
                _root = p;
            } else {
                _scale = 0.0f;
            }
        }
    }
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
    if (_scale < epsilon) {
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
    if (_scale < epsilon)
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

void VoxelModel::setDepth(uint32_t depth) {
    if (depth > _maxDepth) {
        _maxDepth = depth;
    } else if (depth < _maxDepth) {
        struct PruneDepthLevel {
            VoxelOctree* node;
            uint32_t depth;
            void operator() (uint32_t maxDepth) {
                if (depth < maxDepth) {
                    for (auto p : node->subdivisions) {
                        if (p)
                            PruneDepthLevel{ p, depth + 1 }(maxDepth);
                    }
                } else {
                    for (int i = 0; i < 8; ++i) {
                        auto p = node->subdivisions[i];
                        if (p)
                            deleteNode(p);
                        node->subdivisions[i] = nullptr;
                    }
                }
            }
        };
        if (_root) {
            _maxDepth = depth;
            PruneDepthLevel{ _root, 0 }(depth);
        }
    }
}

void VoxelModel::setScale(float scale) {
    FVASSERT_DEBUG(scale > epsilon);
    _scale = std::max(scale, epsilon);
}

AABB VoxelModel::aabb() const {
    if (_scale < epsilon) {
        return AABB::null;
    }
    auto halfExt = Vector3(_scale, _scale, _scale) * 0.5f;
    return AABB{
        _center - halfExt,
        _center + halfExt
    };
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

int VoxelModel::enumerateLevel(int depth, std::function<void(const AABB&, uint32_t, const VoxelOctree&)> cb) const {
    using Callback = std::function<void(const AABB&, uint32_t, const VoxelOctree&)>;

    struct IterateDepth {
        const VoxelOctree& node;
        const AABB aabb;
        uint32_t level;
        uint32_t operator() (uint32_t depth, Callback& cb) {
            if (level < depth) {
                int result = 0;
                node.enumerateSubtree(
                    aabb, [&](const AABB& aabb2, const VoxelOctree& tree) {
                        result += IterateDepth{ tree, aabb2, level + 1 }(depth, cb);
                    });
                return result;
            }
            FVASSERT_DEBUG(level == depth);
            cb(aabb, level, node);
            return 1;
        }
    };
    if (_root) {
        auto volume = this->aabb();
        FVASSERT_DEBUG(volume.isNull() == false);

        if (volume.isNull() == false) {
            return IterateDepth{ *_root, volume, 0 }(depth, cb);
        }
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
    if (_scale < epsilon)
        return 0;

    Vector3 scale = Vector3(_scale, _scale, _scale);
    auto quantize = AffineTransform3::identity.scaled(scale).translated(_center);
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
            float halfExtent = VoxelOctree::halfExtent(depth);
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

constexpr char fileTag[] = "FV.VoxelModel";

bool VoxelModel::deserialize(std::istream& stream) {
    struct {
        char tag[20] = {};
        float bounds[4] = {};
        uint64_t totalNodes = 0;
    } header;

    auto pos = stream.tellg();
    stream.read((char*)&header, sizeof(header));
    if (strcmp(header.tag, fileTag) == 0) {

        _center = Vector3(header.bounds[0], header.bounds[1], header.bounds[2]);
        _scale = header.bounds[3];
        _maxDepth = 0;

        struct Deserializer {
            VoxelOctree* node;
            void operator() (std::istream& stream) {
                stream.read((char*) & (node->value), sizeof(node->value));
                uint8_t subdiv = 0;
                stream.read((char*)&subdiv, sizeof(subdiv));
                for (int i = 0; i < 8; ++i) {
                    if ((subdiv >> i) & 1) {
                        auto p = new VoxelOctree{};
                        node->subdivisions[i] = p;
                        Deserializer{ p }(stream);
                    }
                }
            }
        };

        VoxelOctree* node = nullptr;

        if (header.totalNodes) {
            node = new VoxelOctree{};
            try {
                Deserializer{ node }(stream);
            } catch (const std::ios::failure& fail) {
                Log::error(std::format("IO ERROR! deserialization failed: {}",
                                       fail.what()));
                deleteNode(node);
                node = nullptr;
#if FVCORE_DEBUG_ENABLED
                throw;
#endif
                return false;
            }
        }

        if (_root)
            deleteNode(_root);
        _root = node;
        if (_root)
            _maxDepth = _root->maxDepthLevels();

        return true;
    }
    return false;
}

uint64_t VoxelModel::serialize(std::ostream& stream) const {
    struct Serializer {
        VoxelOctree* node;
        void operator() (std::ostream& stream) const {
            auto subdivMasks = [this]<int... N>(std::integer_sequence<int, N...>) -> uint8_t {
                return ((node->subdivisions[N] ? (1U << N) : 0U) | ...);
            };
            uint8_t subdiv = subdivMasks(std::make_integer_sequence<int, 8>{});

            stream.write((const char*)&node->value, sizeof(node->value));
            stream.write((const char*)&subdiv, sizeof(subdiv));
            for (auto sub : node->subdivisions) {
                if (sub)
                    Serializer{ sub }(stream);
            }
        }
    };

    struct {
        char tag[20] = {};
        float bounds[4] = {};
        uint64_t totalNodes = 0;
    } header;
    strcpy_s(header.tag, std::size(header.tag), fileTag);
    header.bounds[0] = _center.x;
    header.bounds[1] = _center.y;
    header.bounds[2] = _center.z;
    header.bounds[3] = _scale;

    if (_root) {
        header.totalNodes = _root->numDescendants();
    }

    auto pos = stream.tellp();
    stream.write((const char*)& header, sizeof(header));

    if (_root)
        Serializer{ _root }(stream);

    return stream.tellp() - pos;
}
