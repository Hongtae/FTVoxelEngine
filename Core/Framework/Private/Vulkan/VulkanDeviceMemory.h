#pragma once
#include <memory>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanDeviceMemory final {
    public:
        VulkanDeviceMemory(std::shared_ptr<VulkanGraphicsDevice>, VkDeviceMemory, VkMemoryType, VkDeviceSize);
        ~VulkanDeviceMemory();

        VkDeviceMemory memory;
        VkMemoryType type;
        VkDeviceSize length;
        void* mapped;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;

        bool invalidate(uint64_t offset, uint64_t size);
        bool flush(uint64_t offset, uint64_t size);
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
