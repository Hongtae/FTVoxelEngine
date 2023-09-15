#pragma once
#include <memory>
#include "../../PixelFormat.h"
#include "../../GPUBuffer.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "DeviceMemory.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class Buffer : public std::enable_shared_from_this<Buffer> {
    public:
        Buffer(std::shared_ptr<DeviceMemory>, VkBuffer, const VkBufferCreateInfo&);
        Buffer(std::shared_ptr<GraphicsDevice>, VkBuffer, VkDeviceSize);
        ~Buffer();

        void* contents();
        void flush(size_t, size_t);
        size_t length() const;

        std::shared_ptr<class BufferView> makeBufferView(PixelFormat format, size_t offset, size_t range);

        VkBuffer buffer;
        VkBufferUsageFlags  usage;
        VkSharingMode       sharingMode;
        VkDeviceSize        size;

        std::shared_ptr<DeviceMemory> deviceMemory;
        std::shared_ptr<GraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
