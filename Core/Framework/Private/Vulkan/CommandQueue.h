#pragma once
#include <memory>
#include "../../CommandQueue.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>

namespace FV::Vulkan
{
	class GraphicsDevice;
	class QueueFamily;
	class CommandQueue : public FV::CommandQueue, public std::enable_shared_from_this<CommandQueue>
	{
	public:
		CommandQueue(std::shared_ptr<GraphicsDevice>, QueueFamily*, VkQueue);
		~CommandQueue();

		std::shared_ptr<FV::CommandBuffer> makeCommandBuffer() override;
		std::shared_ptr<FV::SwapChain> makeSwapChain(std::shared_ptr<Window>) override;

		bool submit(const VkSubmitInfo* submits, uint32_t submitCount, std::function<void()> callback);

		bool waitIdle();

		uint32_t type() const override;
		std::shared_ptr<FV::GraphicsDevice> device() const override;

		QueueFamily* family;
		VkQueue queue;

		std::shared_ptr<GraphicsDevice> gdevice;
	};
}
#endif //#if FVCORE_ENABLE_VULKAN
