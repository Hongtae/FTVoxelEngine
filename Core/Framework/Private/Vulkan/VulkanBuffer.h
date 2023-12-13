#pragma once
#include <memory>
#include <optional>
#include "../../PixelFormat.h"
#include "../../GPUBuffer.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanDeviceMemory.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanBuffer : public std::enable_shared_from_this<VulkanBuffer> {
    public:
        VulkanBuffer(std::shared_ptr<VulkanGraphicsDevice>, const VulkanMemoryBlock&, VkBuffer, const VkBufferCreateInfo&);
        VulkanBuffer(std::shared_ptr<VulkanGraphicsDevice>, VkBuffer, VkDeviceSize);
        ~VulkanBuffer();

        void* contents();
        void flush(size_t, size_t);
        size_t length() const;

        std::shared_ptr<class VulkanBufferView> makeBufferView(PixelFormat format, size_t offset, size_t range);

        VkBuffer buffer;
        VkBufferUsageFlags  usage;
        VkSharingMode       sharingMode;
        VkDeviceSize        size;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;
        std::optional<VulkanMemoryBlock> memory;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
