#include "AffineTransform3.h"
#include "VoxelModel.h"
#include "DispatchQueue.h"
#include "Logger.h"

using namespace FV;

constexpr auto epsilon = std::numeric_limits<float>::epsilon();

VoxelOctree::VoxelOctree()
    : value({})
    , subdivisions(nullptr)
    , subdivisionMasks(0) {

}

VoxelOctree::VoxelOctree(const Voxel& v)
    : value(v)
    , subdivisions(nullptr)
    , subdivisionMasks(0) {
}

VoxelOctree::VoxelOctree(const VoxelOctree& node) {
    auto n = node.deepCopy();
    value = n.value;
    subdivisionMasks = n.subdivisionMasks;
    subdivisions = n.subdivisions;
    n.subdivisionMasks = 0;
    n.subdivisions = nullptr;
}

VoxelOctree::VoxelOctree(VoxelOctree&& tmp)
    : value(tmp.value)
    , subdivisions(tmp.subdivisions)
    , subdivisionMasks(tmp.subdivisionMasks) {
    tmp.subdivisionMasks = 0;
    tmp.subdivisions = nullptr;
}

VoxelOctree::~VoxelOctree() {
    if (subdivisionMasks) {
        FVASSERT_DEBUG(subdivisions != nullptr);
        delete[] subdivisions;
    }
    subdivisionMasks = 0;
}

VoxelOctree& VoxelOctree::operator=(const VoxelOctree& other) {
    return operator=(static_cast<VoxelOctree&&>(other.deepCopy()));
}

VoxelOctree& VoxelOctree::operator=(VoxelOctree&& tmp) {
    value = tmp.value;
    if (subdivisionMasks) {
        FVASSERT_DEBUG(subdivisions != nullptr);
        delete[] subdivisions;
    }
    subdivisionMasks = tmp.subdivisionMasks;
    subdivisions = tmp.subdivisions;
    tmp.subdivisionMasks = 0;
    tmp.subdivisions = nullptr;
    return *this;
}

VoxelOctree VoxelOctree::deepCopy() const {
    VoxelOctree node(this->value);
    if (subdivisionMasks) {
        auto num = numSubdivisions();
        node.subdivisionMasks = subdivisionMasks;
        node.subdivisions = new VoxelOctree[num];
        for (int i = 0; i < num; ++i) {
            node.subdivisions[i] = this->subdivisions[i];
        }
    }
    return node;
}

void VoxelOctree::subdivide(std::initializer_list<uint8_t> indices) {
    uint8_t mask = subdivisionMasks;
    for (auto i : indices) {
        FVASSERT_DEBUG(i < 8);
        mask = mask | (1 << (i & 7));
    }
    subdivide(mask);
}

void VoxelOctree::subdivide(uint8_t m) {
    uint8_t mask = m | subdivisionMasks;
    auto num = std::popcount(mask);
    auto sub = new VoxelOctree[num](value);
    if (subdivisionMasks) {
        FVASSERT_DEBUG(subdivisions);
        for (int i = 0; i < 8; ++i) {
            if (subdivisionMasks & (1 << i)) {
                uint8_t off1 = std::popcount(subdivisionMasks & ((1U << i) - 1));
                uint8_t off2 = std::popcount(mask & ((1U << i) - 1));
                auto& src = subdivisions[off1];
                auto& dst = sub[off2];
                dst = std::move(src);
            }
        }
        delete subdivisions;
    }
    subdivisions = sub;
    subdivisionMasks = mask;
}

void VoxelOctree::erase(std::initializer_list<uint8_t> indices) {
    uint8_t mask = subdivisionMasks;
    for (auto i : indices) {
        FVASSERT_DEBUG(i < 8);
        mask = mask & ~(1 << (i & 7));
    }
    erase(mask);
}

void VoxelOctree::erase(uint8_t m) {
    uint8_t mask = subdivisionMasks & ~m;
    if (mask) {
        auto num = std::popcount(mask);
        auto sub = new VoxelOctree[num](value);
        if (subdivisionMasks) {
            for (int i = 0; i < 8; ++i) {
                if (subdivisionMasks & (1 << i)) {
                    uint8_t off1 = std::popcount(subdivisionMasks & ((1U << i) - 1));
                    uint8_t off2 = std::popcount(mask & ((1U << i) - 1));
                    auto& src = subdivisions[off1];
                    auto& dst = sub[off2];
                    dst = std::move(src);
                }
            }
            FVASSERT_DEBUG(subdivisions);
            delete subdivisions;
        }
        subdivisions = sub;
    } else {
        if (subdivisionMasks) {
            FVASSERT_DEBUG(subdivisions);
            delete subdivisions;
        }
        subdivisions = nullptr;
    }
    subdivisionMasks = mask;
}

bool VoxelOctree::mergeSolidBranches() {
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    uint16_t a = 0;
    uint16_t metallic = 0;
    uint16_t roughness = 0;
    uint8_t n = 0;

    enumerate([&](uint8_t, const VoxelOctree* p) {
        n++;
        r += p->value.color.r;
        g += p->value.color.g;
        b += p->value.color.b;
        a += p->value.color.a;
        metallic += p->value.metallic;
        roughness += p->value.roughness;
    });

    if (n > 0) {
        this->value.color.r = r / n;
        this->value.color.g = g / n;
        this->value.color.b = b / n;
        this->value.color.a = a / n;
        this->value.metallic = metallic / n;
        this->value.roughness = roughness / n;

        if (n == 8) {
            bool combinable = true;
            enumerate([&](uint8_t, const VoxelOctree* p) {
                if (combinable) {
                    if (p->isLeafNode() == false || p->value != this->value) {
                        combinable = false;
                    }
                }
            });
            if (combinable) {
                delete[] subdivisions;
                subdivisionMasks = 0;
            }
        }
        return true;
    }
    return false;
}

void VoxelOctree::makeSubarray(const Vector3& center,
                               uint32_t depth,
                               const MakeArrayCallback& callback,
                               const VolumePriorityCallback& priority,
                               std::vector<VolumeArray::Node>& vector) const {
    auto index = vector.size();
    vector.push_back({});
    {
        auto& n = vector[index];
        constexpr float q = float(std::numeric_limits<uint16_t>::max());
        n.x = static_cast<uint16_t>(center.x * q);
        n.y = static_cast<uint16_t>(center.y * q);
        n.z = static_cast<uint16_t>(center.z * q);
        n.depth = depth;
        n.flags = 0;
        n.color.value = this->value.color.value;
    }
    if (callback) {
        struct PrioritizedNode {
            const VoxelOctree* node;
            Vector3 center;
            float priority = 0.0f;
        };
        PrioritizedNode children[8] = {};
        int numChildren = 0;

        enumerate(center, depth,
                  [&](const Vector3& pt, uint32_t, const VoxelOctree* p) {
            children[numChildren++] = {
                p, pt, 0.0f
            };
        });

        if (numChildren > 1 && priority) {
            for (int i = 0; i < numChildren; ++i) {
                auto& child = children[i];
                child.priority = priority(child.center, depth + 1);
            }
            std::sort(&children[0], &children[numChildren],
                      [](auto& a, auto& b) {
                return a.priority > b.priority;
            });
        }
        for (int i = 0; i < numChildren; ++i) {
            auto& child = children[i];
            callback(child.center, depth + 1, child.priority, child.node, vector);
        }
    }
    auto advance = (vector.size() - index);
    auto& n = vector[index];
    FVASSERT_DEBUG(advance < std::numeric_limits<decltype(n.advance)>::max());
    n.advance = static_cast<decltype(n.advance)>(advance);
    if (n.advance == 1) { // leaf-node
        n.flags |= VolumeArray::FlagLeafNode;
        n.flags |= VolumeArray::FlagMaterial;
    }
}

void VoxelOctree::makeSubarray(const Vector3& center,
                               uint32_t depth,
                               uint32_t maxDepth,
                               const VolumePriorityCallback& priority,
                               std::vector<VolumeArray::Node>& vector) const {
    if (!priority) {
        return makeSubarray(center, depth, maxDepth, vector);
    }

    auto index = vector.size();
    vector.push_back({});
    {
        auto& n = vector[index];
        constexpr float q = float(std::numeric_limits<uint16_t>::max());
        n.x = static_cast<uint16_t>(center.x * q);
        n.y = static_cast<uint16_t>(center.y * q);
        n.z = static_cast<uint16_t>(center.z * q);
        n.depth = depth;
        n.flags = 0;
        n.color.value = this->value.color.value;
    }
    if (depth < maxDepth) {
        struct PrioritizedNode {
            const VoxelOctree* node;
            Vector3 center;
            float priority = 0.0f;
        };
        PrioritizedNode children[8] = {};
        uint8_t numChildren = 0;

        enumerate(center, depth,
                  [&](const Vector3& pt, uint32_t, const VoxelOctree* p) {
            children[numChildren++] = {
                p, pt, 0.0f
            };
        });

        if (numChildren > 1) {
            for (uint8_t i = 0; i < numChildren; ++i) {
                auto& child = children[i];
                child.priority = priority(child.center, depth + 1);
            }
            std::sort(&children[0], &children[numChildren],
                      [](auto& a, auto& b) {
                return a.priority > b.priority;
            });
        }
        for (uint8_t i = 0; i < numChildren; ++i) {
            auto& child = children[i];
            child.node->makeSubarray(child.center, depth + 1, maxDepth, priority, vector);
        }
    }
    
    auto advance = (vector.size() - index);
    auto& n = vector[index];
    FVASSERT_DEBUG(advance < std::numeric_limits<decltype(n.advance)>::max());
    n.advance = static_cast<decltype(n.advance)>(advance);
    if (n.advance == 1) { // leaf-node
        n.flags |= VolumeArray::FlagLeafNode;
        n.flags |= VolumeArray::FlagMaterial;
    }
}

void VoxelOctree::makeSubarray(const Vector3& center,
                               uint32_t depth,
                               uint32_t maxDepth,
                               std::vector<VolumeArray::Node>& vector) const {
    auto index = vector.size();
    vector.push_back({});
    {
        auto& n = vector[index];
        constexpr float q = float(std::numeric_limits<uint16_t>::max());
        n.x = static_cast<uint16_t>(center.x * q);
        n.y = static_cast<uint16_t>(center.y * q);
        n.z = static_cast<uint16_t>(center.z * q);
        n.depth = depth;
        n.flags = 0;
        n.color.value = this->value.color.value;
    }
    if (depth < maxDepth) {
        enumerate(center, depth,
                  [&](const Vector3& pt, uint32_t depth, const VoxelOctree* p) {
            p->makeSubarray(pt, depth, maxDepth, vector);
        });
    }
    auto advance = (vector.size() - index);
    auto& n = vector[index];
    FVASSERT_DEBUG(advance < std::numeric_limits<decltype(n.advance)>::max());
    n.advance = static_cast<decltype(n.advance)>(advance);
    if (n.advance == 1) { // leaf-node
        n.flags |= VolumeArray::FlagLeafNode;
        n.flags |= VolumeArray::FlagMaterial;
    }
}

VolumeArray VoxelOctree::makeArray(MakeArrayCallback callback) const {
    VolumeArray volume = {};
    volume.aabb = { Vector3::zero, {1,1,1} };
    callback(volume.aabb.center(), 0, 0.0f, this, volume.data);
    //makeSubarray(volume.aabb.center(), 0, volume.data, callback);
    return volume;
}

VolumeArray VoxelOctree::makeArray(uint32_t maxDepth,
                                   MakeArrayFilter filter) const {
    struct MakeArray {
        const VoxelOctree* node;
        const Vector3 center;
        uint32_t depth;
        MakeArrayFilter& filter;
        void operator() (std::vector<VolumeArray::Node>& vector, uint32_t maxDepth) const {

            uint32_t exp = (126U - depth) << 23;
            float halfExtent = std::bit_cast<float>(exp);

            if (filter) {
                AABB aabb = {
                    center - Vector3(halfExtent, halfExtent, halfExtent),
                    center + Vector3(halfExtent, halfExtent, halfExtent)
                };
                filter(aabb, depth, maxDepth);
            }

            auto index = vector.size();
            vector.push_back({});
            {
                auto& n = vector[index];
                constexpr float q = float(std::numeric_limits<uint16_t>::max());
                n.x = static_cast<uint16_t>(center.x * q);
                n.y = static_cast<uint16_t>(center.y * q);
                n.z = static_cast<uint16_t>(center.z * q);
                n.depth = depth;
                n.flags = 0;
                n.color.value = node->value.color.value;
            }

            if (depth < maxDepth) {
                node->enumerate(center, depth, [&]
                (const Vector3& pt, uint32_t depth, const VoxelOctree* p) {
                    MakeArray{ p, pt, depth, filter }(vector, maxDepth);
                });
            }
            auto advance = (vector.size() - index);
            auto& n = vector[index];
            FVASSERT_DEBUG(advance < std::numeric_limits<decltype(n.advance)>::max());
            n.advance = static_cast<decltype(n.advance)>(advance);
            if (n.advance == 1) { // leaf-node
                n.flags |= VolumeArray::FlagLeafNode;
                n.flags |= VolumeArray::FlagMaterial;
            }
        }
    };
    VolumeArray volumes = {};
    volumes.aabb = { Vector3::zero, {1,1,1} };
    //volumes.data.reserve(countNodesToDepth(maxDepth));
    maxDepth = std::clamp(maxDepth, 0U, VoxelOctree::maxDepth);
    MakeArray{ this, Vector3(0.5f, 0.5f, 0.5f), 0, filter }(volumes.data, maxDepth);
    //volumes.data.shrink_to_fit();
    return volumes;
}

VolumeArray VoxelOctree::makeSubarray(const Vector3& center,
                                      uint32_t currentLevel,
                                      uint32_t maxDepth) const {
    auto aabb = AABB{ {0,0,0}, {1,1,1} };
    if (aabb.isPointInside(center) == false)
        return {};

    struct MakeArray {
        const VoxelOctree* node;
        const Vector3 center;
        uint32_t depth;
        void operator() (std::vector<VolumeArray::Node>& vector, uint32_t maxDepth) const {

            uint32_t exp = (126U - depth) << 23;
            float halfExtent = std::bit_cast<float>(exp);

            auto index = vector.size();
            vector.push_back({});
            {
                auto& n = vector[index];
                constexpr float q = float(std::numeric_limits<uint16_t>::max());
                n.x = static_cast<uint16_t>(center.x * q);
                n.y = static_cast<uint16_t>(center.y * q);
                n.z = static_cast<uint16_t>(center.z * q);
                n.depth = depth;
                n.flags = 0;
                n.color.value = node->value.color.value;
            }

            if (depth < maxDepth) {
                node->enumerate(center, depth, [&]
                (const Vector3& pt, uint32_t depth, const VoxelOctree* p) {
                    MakeArray{ p, pt, depth }(vector, maxDepth);
                });
            }
            auto advance = (vector.size() - index);
            auto& n = vector[index];
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
    maxDepth = std::clamp(maxDepth, 0U, VoxelOctree::maxDepth);
    MakeArray{ this, center, currentLevel, }(volumes.data, maxDepth);
    return volumes;
}

VoxelModel::VoxelModel(int depth)
    : _root(nullptr)
    , _maxDepth(std::max(depth, 0)) {
}

VoxelModel::VoxelModel(VoxelOctreeBuilder* builder, int depth)
    : _root(nullptr)
    , _maxDepth(std::max(depth, 0)) {
    if (builder) {
        AABB aabb = builder->aabb();
        if (aabb.isNull() == false) {
            auto extents = aabb.extents();
            auto center = aabb.center();
            auto scale = std::max({ extents.x, extents.y, extents.z });

            if (scale > epsilon) {

                metadata.center = center;
                metadata.scale = scale;

                struct Subdivide {
                    VoxelOctreeBuilder* builder;
                    const Matrix4& transform; // transform to original volume scale
                    Vector3 center; // normalized
                    uint32_t depth;
                    VoxelOctree* node;
                    void operator() (uint32_t maxDepth) const {
                        if (depth < maxDepth) {

                            VoxelOctree* subdivisions[8] = {};
                            int numSubdividions = 0;

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
                                    subdivisions[i] = sub;
                                    numSubdividions++;

                                    Subdivide{
                                        builder, transform,
                                        pt, depth + 1, sub
                                    }(maxDepth);
                                } else {
                                    delete sub;
                                }
                            }

                            if (numSubdividions) {
                                node->subdivisions = new VoxelOctree[numSubdividions];
                                node->subdivisionMasks = 0;
                                int n = 0;
                                for (int i = 0; i < 8; ++i) {
                                    if (auto* p = subdivisions[i]) {
                                        node->subdivisionMasks = node->subdivisionMasks | (1 << i);
                                        node->subdivisions[n++] = std::move(*p);
                                        delete p;
                                    }
                                }
                                FVASSERT_DEBUG(node->subdivisionMasks != 0);
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
                    center - Vector3(scale, scale, scale) * 0.5f,
                    center + Vector3(scale, scale, scale) * 0.5f
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
                    delete p;
                    p = nullptr;
                }
                _root = p;
            }
        }
    }
}

VoxelModel::~VoxelModel() {
    if (_root)
        delete _root;
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

            bool updated = false;
            if ((node->subdivisionMasks & (1 << index)) == 0) {
                if (node->subdivisionMasks == 0) {
                    node->subdivisions = new VoxelOctree[1];
                    node->subdivisionMasks = (1 << index);
                } else {
                    uint8_t numSubs = std::popcount(node->subdivisionMasks);
                    VoxelOctree* subdivisions = new VoxelOctree[numSubs + 1];
                    uint8_t masks = node->subdivisionMasks | (1 << index);
                    FVASSERT_DEBUG(masks != node->subdivisionMasks);
                    for (int i = 0; i < 8; ++i) {
                        if (node->subdivisionMasks & (1 << i)) {
                            uint32_t m1 = node->subdivisionMasks & ((1 << i) - 1);
                            uint32_t m2 = masks & ((1 << i) - 1);
                            auto& src = node->subdivisions[std::popcount(m1)];
                            auto& dst = subdivisions[std::popcount(m2)];
                            dst = std::move(src);
                        }
                    }
                    delete[] node->subdivisions;
                    node->subdivisions = subdivisions;
                    node->subdivisionMasks = masks;
                }
                updated = true; // A new item has been added.
            }
            VoxelOctree* p = nullptr;
            if (1) {
                uint32_t mask = node->subdivisionMasks & ((1 << index) - 1);
                uint32_t offset = std::popcount(mask);
                p = node->subdivisions + offset;
            }
            FVASSERT_DEBUG(p);

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
                uint8_t index = ((nz & 1) << 2) | ((ny & 1) << 1) | (nx & 1);

                if (dim > 1) {
                    if (node->isLeafNode()) {
                        node->subdivide(0xff);
                    }
                    if (node->subdivisionMasks & (1 << index)) {
                        uint32_t offset = std::popcount(node->subdivisionMasks & ((1U << index) - 1));
                        auto p = node->subdivisions + offset;

                        if (EraseNode{ dim >> 1, p }(x % dim, y % dim, z % dim)) {
                            if (p->isLeafNode()) {
                                node->erase({ index });
                            }
                            node->mergeSolidBranches();
                            return true;
                        }
                    }
                } else {
                    if (node->subdivisionMasks & (1 << index)) {
                        uint32_t offset = std::popcount(node->subdivisionMasks & ((1U << index) - 1));
                        auto& n = node->subdivisions[offset];
                        FVASSERT_DEBUG(n.isLeafNode());
                        node->erase({ index });
                        node->mergeSolidBranches();
                        return true;
                    } else {
                        if (node->isLeafNode()) {
                            uint8_t mask = 0;
                            for (int i = 0; i < 8; ++i) {
                                if (i != index)
                                    mask = mask | (1 << i);
                            }
                            node->subdivide(mask);
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
                    delete _root;
                    _root = nullptr;
                } else {
                    _root->mergeSolidBranches();
                }
            }
        } else {
            FVASSERT_DEBUG(_root->isLeafNode());
            delete _root;
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
            if (node->subdivisionMasks & (1U << index)) {
                uint32_t offset = std::popcount(node->subdivisionMasks & ((1U << index) - 1));
                auto p = node->subdivisions + offset;
                return Lookup{ dim >> 1, p }(x % dim, y % dim, z % dim);
            }
            return node;
        }
    };
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
                    node->enumerate([&](uint32_t, VoxelOctree* p) {
                        PruneDepthLevel{ p, depth + 1 }(maxDepth);
                    });
                } else {
                    node->erase(0xff);
                    FVASSERT_DEBUG(node->isLeafNode());
                }
            }
        };
        if (_root) {
            _maxDepth = depth;
            PruneDepthLevel{ _root, 0 }(depth);
        }
    }
}

void VoxelModel::optimize() {
    struct ForEachNode {
        VoxelOctree* node;
        void operator() (std::function<void(VoxelOctree*)>& callback) {
            node->enumerate([&](uint8_t, VoxelOctree* p) {
                ForEachNode{ p }(callback);
            });
            callback(node);
        }
    };
    std::function<void(VoxelOctree*)> fn = [&](VoxelOctree* node) {
        node->mergeSolidBranches();
    };
    ForEachNode{ _root }(fn);
}

int VoxelModel::enumerateLevel(int depth, std::function<void(const AABB&, uint32_t, const VoxelOctree*)> cb) const {
    using Callback = std::function<void(const AABB&, uint32_t, const VoxelOctree*)>;

    struct IterateDepth {
        const VoxelOctree* node;
        const AABB aabb;
        uint32_t level;
        uint32_t operator() (uint32_t depth, Callback& cb) {
            if (level < depth) {
                int result = 0;
                node->enumerate(
                    aabb, [&](const AABB& aabb2, const VoxelOctree* tree) {
                        result += IterateDepth{ tree, aabb2, level + 1 }(depth, cb);
                    });
                if (result > 0)
                    return result;
            }
            // The level is equal to the depth, or no more children exist at the current level.
            cb(aabb, level, node);
            return 1;
        }
    };
    if (_root) {
        auto volume = AABB{ {0,0,0}, {1,1,1} };
        return IterateDepth{ _root, volume, 0 }(depth, cb);
    }
    return 0;
}

int VoxelModel::enumerateLevel(int depth, std::function<void(const Vector3&, uint32_t, const VoxelOctree*)> cb) const {
    using Callback = std::function<void(const Vector3&, uint32_t, const VoxelOctree*)>;

    struct IterateDepth {
        const VoxelOctree* node;
        const Vector3 center;
        uint32_t level;
        uint32_t operator() (uint32_t depth, Callback& cb) {
            if (level < depth) {
                int result = 0;
                node->enumerate(
                    center, level, [&](const Vector3& center, uint32_t level, const VoxelOctree* tree) {
                    result += IterateDepth{ tree, center, level }(depth, cb);
                });
                if (result > 0)
                    return result;
            }
            // The level is equal to the depth, or no more children exist at the current level.
            cb(center, level, node);
            return 1;
        }
    };
    if (_root) {
        return IterateDepth{ _root, {0.5f, 0.5f, 0.5f}, 0}(depth, cb);
    }
    return 0;
}

std::optional<VoxelModel::RayHitResult> VoxelModel::rayTest(const Vector3& rayOrigin, const Vector3& dir, RayHitResultOption option) const {
    std::optional<RayHitResult> rayHit = {};
    if (option == CloestHit) {
        auto numHits = rayTest(
            rayOrigin, dir, [&](const auto& p2) {
                if (rayHit) {
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
                if (rayHit) {
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
    struct RayTestNode {
        const VoxelOctree* node;
        const Vector3 center;
        const uint32_t depth;
        uint32_t resolution;
        bool& continueRayTest;
        std::function<bool(const RayHitResult&)>& callback;
        uint64_t rayTest(const Vector3& start, const Vector3& dir) const {
            float halfExtent = VoxelOctree::halfExtent(depth);
            AABB aabb = {
                center - Vector3(halfExtent, halfExtent, halfExtent),
                center + Vector3(halfExtent, halfExtent, halfExtent)
            };
            auto r = aabb.rayTest(start, dir);
            if (r >= 0.0f) {
                uint64_t numHits = 0;
                
                node->enumerate(center, depth, [&]
                (const Vector3& pt, uint32_t depth, const VoxelOctree* p) {
                    if (continueRayTest) {
                        numHits += RayTestNode{ p, pt, depth, resolution, continueRayTest, callback }
                        .rayTest(start, dir);
                    }
                });
                if (node->isLeafNode()) {
                    uint32_t x = (uint32_t)std::floor(center.x * resolution);
                    uint32_t y = (uint32_t)std::floor(center.y * resolution);
                    uint32_t z = (uint32_t)std::floor(center.z * resolution);
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
        callback = filter;
    } else {
        callback = [](auto&&) { return true; };
    }

    return RayTestNode{
        _root, Vector3(0.5f, 0.5f, 0.5f), 0, resolution(), continueRayTest, callback 
    }.rayTest(rayOrigin, dir);
}

constexpr char fileTag[] = "FV.VoxelModel";
constexpr size_t voxelSize = 8;
static_assert(sizeof(Voxel) <= voxelSize);

bool VoxelModel::deserialize(std::istream& stream) {
    struct {
        char tag[20] = {};
        float bounds[4] = {};
        uint64_t totalNodes = 0;
    } header;

    auto pos = stream.tellg();
    stream.read((char*)&header, sizeof(header));
    if (strcmp(header.tag, fileTag) == 0) {

        auto center = Vector3(header.bounds[0], header.bounds[1], header.bounds[2]);
        auto scale = header.bounds[3];
        _maxDepth = 0;

        struct Deserializer {
            VoxelOctree* node;
            void operator() (std::istream& stream) {
                char buff[voxelSize] = {};
                stream.read(buff, voxelSize);
                memcpy(&(node->value), buff, sizeof(node->value));
                uint8_t subdiv = 0;
                stream.read((char*)&subdiv, sizeof(subdiv));

                if (subdiv) {
                    VoxelOctree* sub = new VoxelOctree[std::popcount(subdiv)];

                    for (int i = 0; i < 8; ++i) {
                        if ((subdiv >> i) & 1) {
                            uint32_t offset = std::popcount(subdiv & ((1U << i) - 1));
                            auto p = sub + offset;
                            Deserializer{ p }(stream);
                        }
                    }
                    node->subdivisions = sub;
                }
                node->subdivisionMasks = subdiv;
            }
        };

        VoxelOctree* node = nullptr;

        if (header.totalNodes) {
            node = new VoxelOctree{};
            try {
                Deserializer{ node }(stream);
            } catch (const std::ios::failure& fail) {
                Log::error("IO ERROR! deserialization failed: {}", fail.what());
                delete node;
                node = nullptr;
#if FVCORE_DEBUG_ENABLED
                throw;
#endif
                return false;
            }
        }

        if (_root)
            delete _root;
        _root = node;
        if (_root)
            _maxDepth = _root->maxDepthLevels();

        metadata.center = center;
        metadata.scale = scale;
        return true;
    }
    return false;
}

uint64_t VoxelModel::serialize(std::ostream& stream) const {
    struct Serializer {
        const VoxelOctree* node;
        void operator() (std::ostream& stream) const {
            uint8_t subdiv = node->subdivisionMasks;

            char buff[voxelSize] = {};
            memcpy(buff, (const char*)&node->value, sizeof(node->value));
            stream.write(buff, voxelSize);
            stream.write((const char*)&subdiv, sizeof(subdiv));

            node->enumerate([&](uint8_t, const VoxelOctree* p) {
                Serializer{ p }(stream);
            });
        }
    };

    struct {
        char tag[20] = {};
        float bounds[4] = {};
        uint64_t totalNodes = 0;
    } header;
    strcpy_s(header.tag, std::size(header.tag), fileTag);
    header.bounds[0] = metadata.center.x;
    header.bounds[1] = metadata.center.y;
    header.bounds[2] = metadata.center.z;
    header.bounds[3] = metadata.scale;

    if (_root) {
        header.totalNodes = _root->numDescendants();
    }

    auto pos = stream.tellp();
    stream.write((const char*)& header, sizeof(header));

    if (_root)
        Serializer{ _root }(stream);

    return stream.tellp() - pos;
}
