#pragma once
#include <memory>

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan {
    class GraphicsDevice;
    class DeviceMemory final {
    public:
        DeviceMemory(std::shared_ptr<GraphicsDevice>, VkDeviceMemory, VkMemoryType, VkDeviceSize);
        ~DeviceMemory();

        VkDeviceMemory memory;
        VkMemoryType type;
        VkDeviceSize length;
        void* mapped;

        std::shared_ptr<GraphicsDevice> gdevice;

        bool invalidate(uint64_t offset, uint64_t size);
        bool flush(uint64_t offset, uint64_t size);
    };
}

#endif //#if FVCORE_ENABLE_VULKAN
