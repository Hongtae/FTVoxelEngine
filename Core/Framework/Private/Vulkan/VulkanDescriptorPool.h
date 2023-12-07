#pragma once
#include <algorithm>
#include <iterator>
#include "../../Hash.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"

namespace FV {
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

    struct VulkanDescriptorPoolID {
        uint32_t mask = 0;
        uint32_t typeSize[numDescriptorTypes] = { 0 };

        uint32_t hash() const {
            CRC32 crc32;
            crc32.update(&mask, sizeof(mask));
            crc32.update(&typeSize, sizeof(typeSize));
            return crc32.finalize().hash;
        }

        VulkanDescriptorPoolID() = default;
        VulkanDescriptorPoolID(uint32_t poolSizeCount, const VkDescriptorPoolSize* poolSizes)
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
        VulkanDescriptorPoolID(const ShaderBindingSetLayout& layout)
            : mask(0)
            , typeSize{ 0 } {
            for (const ShaderBinding& binding : layout.bindings) {
                VkDescriptorType type = getVkDescriptorType(binding.type);
                auto typeIndex = indexOfDescriptorType(type);
                typeSize[typeIndex] += binding.arrayLength;
            }
            uint32_t dpTypeKey = 0;
            for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                if (typeSize[i])
                    mask = mask | (1 << i);
            }
        }
        bool operator < (const VulkanDescriptorPoolID& rhs) const {
            if (this->mask == rhs.mask) {
                for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                    if (this->typeSize[i] != rhs.typeSize[i])
                        return this->typeSize[i] < rhs.typeSize[i];
                }
            }
            return this->mask < rhs.mask;
        }
        bool operator > (const VulkanDescriptorPoolID& rhs) const {
            if (this->mask == rhs.mask) {
                for (uint32_t i = 0; i < numDescriptorTypes; ++i) {
                    if (this->typeSize[i] != rhs.typeSize[i])
                        return this->typeSize[i] > rhs.typeSize[i];
                }
            }
            return this->mask > rhs.mask;
        }
        bool operator == (const VulkanDescriptorPoolID& rhs) const {
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

    class VulkanGraphicsDevice;
    class VulkanDescriptorPool final {
    public:
        const VulkanDescriptorPoolID& poolID; // key for container(DescriptorPoolChain)
        const uint32_t maxSets;
        const VkDescriptorPoolCreateFlags createFlags;

        VkDescriptorPool pool;
        VulkanGraphicsDevice* gdevice;
        uint32_t numAllocatedSets;


        VulkanDescriptorPool(VulkanGraphicsDevice*, VkDescriptorPool, const VkDescriptorPoolCreateInfo& ci, const VulkanDescriptorPoolID&);
        ~VulkanDescriptorPool();

        VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout);

        void releaseDescriptorSets(VkDescriptorSet*, uint32_t);

    };
}
#endif //#if FVCORE_ENABLE_VULKAN
