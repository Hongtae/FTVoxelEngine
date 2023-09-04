#pragma once
#include <algorithm>
#include <iterator>
#include "../../Hash.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "Types.h"

namespace FV::Vulkan {
    constexpr VkDescriptorType descriptorTypes[] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK
    };
    static_assert(std::is_sorted(std::begin(descriptorTypes), std::end(descriptorTypes)));
    constexpr size_t numDescriptorTypes = std::size(descriptorTypes);

    inline auto indexOfDescriptorType(VkDescriptorType t) {
        auto it = std::lower_bound(std::begin(descriptorTypes),
                                   std::end(descriptorTypes),
                                   t);
        FVASSERT_DEBUG(it != std::end(descriptorTypes));
        return std::distance(std::begin(descriptorTypes), it);
    };
    template <typename Index> inline
        VkDescriptorType descriptorTypeAtIndex(Index index) {
        FVASSERT_DEBUG(index >= 0 && index < numDescriptorTypes);
        auto it = std::begin(descriptorTypes);
        std::advance(it, index);
        return *it;
    }

    struct DescriptorPoolID {
        uint32_t mask = 0;
        uint32_t typeSize[numDescriptorTypes] = { 0 };

        uint32_t hash() const {
            CRC32 crc32;
            crc32.update(&mask, sizeof(mask));
            crc32.update(&typeSize, sizeof(typeSize));
            return crc32.finalize().hash;
        }

        DescriptorPoolID() = default;
        DescriptorPoolID(uint32_t poolSizeCount, const VkDescriptorPoolSize* poolSizes)
            : mask(0)
            , typeSize{ 0 } {
            for (uint32_t i = 0; i < poolSizeCount; ++i) {
                const VkDescriptorPoolSize& poolSize = poolSizes[i];
                auto typeIndex = indexOfDescriptorType(poolSize.type);
                typeSize[typeIndex] += poolSize.descriptorCount;
            }
            uint32_t dpTypeKey = 0;
            for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                if (typeSize[i])
                    mask = mask | (1 << i);
            }
        }
        DescriptorPoolID(const ShaderBindingSetLayout& layout)
            : mask(0)
            , typeSize{ 0 } {
            for (const ShaderBinding& binding : layout.bindings) {
                VkDescriptorType type = getDescriptorType(binding.type);
                auto typeIndex = indexOfDescriptorType(type);
                typeSize[typeIndex] += binding.arrayLength;
            }
            uint32_t dpTypeKey = 0;
            for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                if (typeSize[i])
                    mask = mask | (1 << i);
            }
        }
        bool operator < (const DescriptorPoolID& rhs) const {
            if (this->mask == rhs.mask) {
                for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                    if (this->typeSize[i] != rhs.typeSize[i])
                        return this->typeSize[i] < rhs.typeSize[i];
                }
            }
            return this->mask < rhs.mask;
        }
        bool operator > (const DescriptorPoolID& rhs) const {
            if (this->mask == rhs.mask) {
                for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                    if (this->typeSize[i] != rhs.typeSize[i])
                        return this->typeSize[i] > rhs.typeSize[i];
                }
            }
            return this->mask > rhs.mask;
        }
        bool operator == (const DescriptorPoolID& rhs) const {
            if (this->mask == rhs.mask) {
                for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                    if (this->typeSize[i] != rhs.typeSize[i])
                        return false;
                }
                return true;
            }
            return false;
        }
    };

    class GraphicsDevice;
    class DescriptorPool final {
    public:
        const DescriptorPoolID& poolID; // key for container(DescriptorPoolChain)
        const uint32_t maxSets;
        const VkDescriptorPoolCreateFlags createFlags;

        VkDescriptorPool pool;
        GraphicsDevice* gdevice;
        uint32_t numAllocatedSets;


        DescriptorPool(GraphicsDevice*, VkDescriptorPool, const VkDescriptorPoolCreateInfo& ci, const DescriptorPoolID&);
        ~DescriptorPool();

        VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout);

        void releaseDescriptorSets(VkDescriptorSet*, uint32_t);

    };
}
#endif //#if FVCORE_ENABLE_VULKAN
