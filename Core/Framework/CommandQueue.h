#pragma once
#include "../include.h"
#include <memory>
#include "RenderPass.h"
#include "CommandBuffer.h"
#include "SwapChain.h"

namespace FV
{
    class Window;
	class GraphicsDevice;
    class CommandQueue
    {
	public:
		enum TypeFlags : uint32_t
		{
			Copy = 0,
			Render = 1,
            Compute = 1 << 1,
		};

		virtual ~CommandQueue() {}

		virtual std::shared_ptr<CommandBuffer> makeCommandBuffer() = 0;
		virtual std::shared_ptr<SwapChain> makeSwapChain(std::shared_ptr<Window>) = 0;

		virtual uint32_t flags() const = 0;
		virtual std::shared_ptr<GraphicsDevice> device() const = 0;
    };

	inline std::shared_ptr<GraphicsDevice> CommandBuffer::device() const
	{
		return queue()->device();
	}
}
