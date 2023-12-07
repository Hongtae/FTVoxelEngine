#pragma once
#include <memory>
#include "../../GPUBuffer.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanBuffer.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanBufferView : public GPUBuffer {
    public:
        VulkanBufferView(std::shared_ptr<VulkanBuffer>);
        VulkanBufferView(std::shared_ptr<VulkanBuffer>, VkBufferView, const VkBufferViewCreateInfo&);
        VulkanBufferView(std::shared_ptr<VulkanGraphicsDevice>, VkBufferView);
        ~VulkanBufferView();

        void* contents() override {
            return buffer->contents();
        }
        void flush() override {
            buffer->flush(0, VK_WHOLE_SIZE);
        }
        size_t length() const override {
            return buffer->length();
        }

        std::shared_ptr<GraphicsDevice> device() const override;

        VkBufferView bufferView;
        std::shared_ptr<VulkanBuffer> buffer;
        std::shared_ptr<VulkanGraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
