#pragma once
#include <memory>
#include "../../GPUBuffer.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "Buffer.h"

namespace FV::Vulkan
{
    class GraphicsDevice;
    class BufferView : public FV::GPUBuffer
    {
    public:
        BufferView(std::shared_ptr<Buffer>);
        BufferView(std::shared_ptr<Buffer>, VkBufferView, const VkBufferViewCreateInfo&);
        BufferView(std::shared_ptr<GraphicsDevice>, VkBufferView);
        ~BufferView();

        void* contents() override
        {
            return buffer->contents();
        }
        void flush() override
        {
            buffer->flush(0, VK_WHOLE_SIZE);
        }
        size_t length() const override
        {
            return buffer->length();
        }

        std::shared_ptr<FV::GraphicsDevice> device() const override;

        VkBufferView bufferView;
        std::shared_ptr<Buffer> buffer;
        std::shared_ptr<GraphicsDevice> gdevice;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
