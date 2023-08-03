#include "CommandQueue.h"
#include "CommandBuffer.h"
#include "GraphicsDevice.h"
#include "SwapChain.h"

#if FVCORE_ENABLE_VULKAN
using namespace FV::Vulkan;

CommandQueue::CommandQueue(std::shared_ptr<GraphicsDevice> d, QueueFamily* f, VkQueue q)
	: gdevice(d)
	, family(f)
	, queue(q)	
{
}

CommandQueue::~CommandQueue()
{
	vkQueueWaitIdle(queue);
	family->recycleQueue(queue);
}

std::shared_ptr<FV::CommandBuffer> CommandQueue::makeCommandBuffer()
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmdPoolCreateInfo.queueFamilyIndex = this->family->familyIndex;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkResult err = vkCreateCommandPool(gdevice->device, &cmdPoolCreateInfo, gdevice->allocationCallbacks(), &commandPool);
	if (err != VK_SUCCESS)
	{
		Log::error(std::format("vkCreateCommandPool failed: {}", getVkResultString(err)));
		return nullptr;
	}

	std::shared_ptr<CommandBuffer> buffer = std::make_shared<CommandBuffer>(shared_from_this(), commandPool);
	return buffer;
}

std::shared_ptr<FV::SwapChain> CommandQueue::makeSwapChain(std::shared_ptr<Window> window)
{
	auto swapChain = std::make_shared<SwapChain>(shared_from_this(), window);
	if (swapChain->setup())
	{
		if (!this->family->supportPresentation)
		{
			auto& physicalDevice = gdevice->physicalDevice;

			VkBool32 supported = VK_FALSE;
			VkResult err = gdevice->instance->extensionProc
				.vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.device,
													  this->family->familyIndex,
													  swapChain->surface,
													  &supported);
			if (err != VK_SUCCESS)
			{
				Log::error(std::format("vkGetPhysicalDeviceSurfaceSupportKHR failed: {}", getVkResultString(err)));
				return nullptr;
			}
			if (!supported)
			{
				Log::error("Vulkan WSI not supported with this queue family. Try to use other queue family!");
				return nullptr;
			}
		}
		return swapChain;
	}
	return nullptr;
}

bool CommandQueue::submit(const VkSubmitInfo* submits, uint32_t submitCount, std::function<void()> callback)
{
	VkFence fence = VK_NULL_HANDLE;
	if (callback)
		fence = gdevice->getFence();

	VkResult err = vkQueueSubmit(queue, submitCount, submits, fence);
	if (err != VK_SUCCESS)
	{
		Log::error(std::format("vkQueueSubmit failed: {}", getVkResultString(err)));
		FVASSERT(err == VK_SUCCESS);
	}
	if (fence)
		gdevice->addFenceCompletionHandler(fence, callback);

	return err == VK_SUCCESS;
}

bool CommandQueue::waitIdle()
{
	return vkQueueWaitIdle(queue) == VK_SUCCESS;
}

uint32_t CommandQueue::flags() const
{
	VkQueueFlags queueFlags = family->properties.queueFlags;
	uint32_t flags = Copy; /* Copy = 0 */
	if (queueFlags & VK_QUEUE_GRAPHICS_BIT)
		flags = flags | Render;
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
		flags = flags | Compute;
	return flags;
}

std::shared_ptr<FV::GraphicsDevice> CommandQueue::device() const
{
	return gdevice;
}
#endif //#if FVCORE_ENABLE_VULKAN
