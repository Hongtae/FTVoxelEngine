#include <chrono>
#include "../../Logger.h"
#include "../../Hash.h"
#include "GraphicsDevice.h"
#include "CommandQueue.h"
#include "ShaderModule.h"
#include "ShaderFunction.h"
#include "ShaderBindingSet.h"
#include "RenderPipelineState.h"
#include "ComputePipelineState.h"
#include "DepthStencilState.h"
#include "DeviceMemory.h"
#include "BufferView.h"
#include "ImageView.h"
#include "Semaphore.h"
#include "TimelineSemaphore.h"
#include "Types.h"

#if FVCORE_ENABLE_VULKAN
namespace 
{
    struct _ScopeExit
    {
        std::function<void()> fn;
        //_ScopeExit(std::function<void()> f) : fn(f) {}
        ~_ScopeExit() { fn(); }
    };
}

using namespace FV::Vulkan;

GraphicsDevice::GraphicsDevice(std::shared_ptr<VulkanInstance> ins,
                               const PhysicalDeviceDescription& pd,
                               std::vector<std::string> requiredExtensions,
                               std::vector<std::string> optionalExtensions)
    : instance(ins)
    , physicalDevice(pd)
    , device(VK_NULL_HANDLE)
    , pipelineCache(VK_NULL_HANDLE)
    , numberOfFences(0)
    , extensionProc{}
{
    std::vector<float> queuePriority = std::vector<float>(physicalDevice.maxQueues, 0.0f);

    // setup queue
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(physicalDevice.queueFamilies.size());

    for (uint32_t index = 0; index < physicalDevice.queueFamilies.size(); ++index)
    {
        const auto& queueFamily = physicalDevice.queueFamilies.at(index);
        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = index;
        queueInfo.queueCount = queueFamily.queueCount;
        queueInfo.pQueuePriorities = queuePriority.data();
        queueCreateInfos.push_back(queueInfo);
    }
    if (queueCreateInfos.empty())
    {
        Log::error("No queues in PhysicalDevice");
        throw std::runtime_error("No queues in PhysicalDevice!");
    }

    requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    requiredExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    requiredExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
    //requiredExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

    optionalExtensions.push_back(VK_KHR_MAINTENANCE_2_EXTENSION_NAME);
    optionalExtensions.push_back(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
    optionalExtensions.push_back(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);

    // setup extensions
    std::vector<const char*> deviceExtensions;
    deviceExtensions.reserve(requiredExtensions.size() + optionalExtensions.size());
    for (auto& ext : requiredExtensions)
    {
        deviceExtensions.push_back(ext.c_str());
        if (physicalDevice.hasExtension(ext) == false)
        {
            Log::warning(std::format(
                "Vulkan device extension: \"{}\" not supported, but required.",
                ext));
        }
    }
    for (auto& ext : optionalExtensions)
    {
        if (physicalDevice.hasExtension(ext))
            deviceExtensions.push_back(ext.c_str());
        else
        {
            Log::warning(std::format(
                "Vulkan device extension: \"{}\" not supported.", ext));
        }
    }

    // features
    VkPhysicalDeviceFeatures enabledFeatures = physicalDevice.features; // enable all features supported by a device
    VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
    };
    deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

    if (deviceExtensions.empty() == false)
    {
        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    auto deviceExtensionContains = [&](const char* ext)-> bool
    {
        return std::find_if(deviceExtensions.begin(), deviceExtensions.end(),
                            [ext](const char* str)
                            {
                                return strcmp(ext, str) == 0;
                            }) != deviceExtensions.end();
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extDynamicFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
    };
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extDynamic2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT
    };
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynamic3Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
    };
    VkPhysicalDeviceMaintenance4Features maintenance4Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES 
    };

    if (deviceExtensionContains(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME))
    {
        extDynamicFeatures.extendedDynamicState = VK_TRUE;
        appendNextChain(&deviceCreateInfo, &extDynamicFeatures);
    }
    if (deviceExtensionContains(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME))
    {
        extDynamic2Features.extendedDynamicState2 = VK_TRUE;
        appendNextChain(&deviceCreateInfo, &extDynamic2Features);
    }
    if (deviceExtensionContains(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME))
    {
        extDynamic3Features.extendedDynamicState3DepthClampEnable = VK_TRUE;
        extDynamic3Features.extendedDynamicState3PolygonMode = VK_TRUE;
        extDynamic3Features.extendedDynamicState3DepthClipEnable = VK_TRUE;
        appendNextChain(&deviceCreateInfo, &extDynamic3Features);
    }
    if (deviceExtensionContains(VK_KHR_MAINTENANCE_4_EXTENSION_NAME))
    {
        maintenance4Features.maintenance4 = VK_TRUE;
        appendNextChain(&deviceCreateInfo, &maintenance4Features);
    }

    VkDevice device = VK_NULL_HANDLE;
    VkResult err = vkCreateDevice(physicalDevice.device, &deviceCreateInfo,
                                  instance->allocationCallback, &device);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateDevice failed: {}",
                               getVkResultString(err)));
        throw std::runtime_error("vkCreateDevice failed");
    }
    this->device = device;
    this->extensionProc.load(device);

    this->deviceMemoryTypes.resize(physicalDevice.memory.memoryTypeCount);
    for (uint32_t i = 0; i < physicalDevice.memory.memoryTypeCount; ++i)
    {
        this->deviceMemoryTypes.at(i) = physicalDevice.memory.memoryTypes[i];
    }
    this->deviceMemoryHeaps.resize(physicalDevice.memory.memoryHeapCount);
    for (uint32_t i = 0; i < physicalDevice.memory.memoryHeapCount; ++i)
    {
        this->deviceMemoryHeaps.at(i) = physicalDevice.memory.memoryHeaps[i];
    }

    this->queueFamilies.clear();
    this->queueFamilies.reserve(queueCreateInfos.size());
    for (VkDeviceQueueCreateInfo& queueInfo : queueCreateInfos)
    {
        bool supportPresentation = false;
#if VK_USE_PLATFORM_WIN32_KHR
        supportPresentation = instance->extensionProc
            .vkGetPhysicalDeviceWin32PresentationSupportKHR(
                physicalDevice.device, queueInfo.queueFamilyIndex) != VK_FALSE;
#endif
#if VK_USE_PLATFORM_ANDROID_KHR
        supportPresentation = true;  // always true on Android
#endif
        VkQueueFamilyProperties properties = physicalDevice.queueFamilies[queueInfo.queueFamilyIndex];
        auto queue = new QueueFamily(device,
                                     queueInfo.queueFamilyIndex,
                                     queueInfo.queueCount,
                                     properties,
                                     supportPresentation);
        this->queueFamilies.push_back(queue);
    }
    std::sort(this->queueFamilies.begin(), this->queueFamilies.end(),
              [](auto& lhs, auto& rhs)
              {
                  int lp = lhs->supportPresentation ? 1 : 0;
                  int rp = rhs->supportPresentation ? 1 : 0;
                  if (lp == rp) // smaller index first
                      return lhs->familyIndex < rhs->familyIndex;
                  return lp > rp;
              });
    this->queueFamilies.shrink_to_fit();

    this->loadPipelineCache();

    // launch thread.
    this->fenceCompletionThread = std::jthread(
        [this](std::stop_token stopToken)
        {
            fenceCompletionCallbackThreadProc(stopToken);
        });
}

GraphicsDevice::~GraphicsDevice()
{
    fenceCompletionThread.request_stop();
    fenceCompletionCond.notify_all();
    fenceCompletionThread.join();

    for (int i = 0; i < NumDescriptorPoolChainBuckets; ++i)
    {
        auto& chainMap = descriptorPoolChainMaps[i].poolChainMap;
        for (auto& cmap : chainMap)
        {
            DescriptorPoolChain* chain = cmap.second;
            for (auto& pool : chain->descriptorPools)
            {
                FVASSERT_DEBUG(pool->numAllocatedSets == 0);
            }
            delete chain;
        }
        chainMap.clear();
    }

    vkDeviceWaitIdle(device);

    FVASSERT_DEBUG(pendingFenceCallbacks.empty());
    for (VkFence fence : reusableFences)
        vkDestroyFence(device, fence, allocationCallbacks());
    reusableFences.clear();

	for (QueueFamily* family : queueFamilies)
	{
		delete family;
	}
	queueFamilies.clear();

	// destroy pipeline cache
	if (pipelineCache != VK_NULL_HANDLE)
	{
		vkDestroyPipelineCache(device, pipelineCache, allocationCallbacks());
		this->pipelineCache = VK_NULL_HANDLE;
	}

	vkDestroyDevice(device, allocationCallbacks());
    device = VK_NULL_HANDLE;
}

std::shared_ptr<FV::CommandQueue> GraphicsDevice::makeCommandQueue(uint32_t flags)
{
	uint32_t queueFlags = 0;
	if (flags & CommandQueue::Render)
		queueFlags = queueFlags | VK_QUEUE_GRAPHICS_BIT;
	if (flags & CommandQueue::Compute)
		queueFlags = queueFlags | VK_QUEUE_COMPUTE_BIT;
    uint32_t queueMask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
	queueMask = queueMask ^ queueFlags;

	// find the exact matching queue
	for (QueueFamily* family : queueFamilies)
	{
		if ((family->properties.queueFlags & queueMask) == 0 &&
			(family->properties.queueFlags & queueFlags) == queueFlags)
		{
			std::shared_ptr<FV::CommandQueue> queue = family->makeCommandQueue(shared_from_this());
			if (queue)
				return queue;
		}
	}

	// find any queue that satisfies the condition.
	for (QueueFamily* family : queueFamilies)
	{
		if ((family->properties.queueFlags & queueFlags) == queueFlags)
		{
			std::shared_ptr<FV::CommandQueue> queue = family->makeCommandQueue(shared_from_this());
			if (queue)
				return queue;
		}
	}
	return nullptr;
}

std::shared_ptr<FV::ShaderModule> GraphicsDevice::makeShaderModule(const FV::Shader& shader)
{
    if (shader.isValid() == false)
        return nullptr;

    uint32_t maxPushConstantsSize = this->properties().limits.maxPushConstantsSize;

    for (auto& layout : shader.pushConstantLayouts())
    {
        if (layout.offset >= maxPushConstantsSize)
        {
            Log::error(std::format(
                "PushConstant offset is out of range. (offset: {:d}, limit: {:d})",
                                   layout.offset, maxPushConstantsSize));
            return nullptr;
        }
        if (layout.offset + layout.size > maxPushConstantsSize)
        {
            Log::error(std::format(
                "PushConstant range exceeded limit. (offset: {:d}, size: {:d}, limit: {:d})",
                                   layout.offset, layout.size, maxPushConstantsSize));
            return nullptr;
        }
    }

    uint32_t* maxComputeWorkgroupSize = this->properties().limits.maxComputeWorkGroupSize;
    auto threadWorkgroupSize = shader.threadgroupSize();
    if (threadWorkgroupSize.x > maxComputeWorkgroupSize[0] ||
        threadWorkgroupSize.y > maxComputeWorkgroupSize[1] ||
        threadWorkgroupSize.z > maxComputeWorkgroupSize[2])
    {
        Log::error(std::format(
            "Thread-WorkGroup size exceeded limit. Size:({:d},{:d},{:d}), Limit:({:d},{:d},{:d})",
            threadWorkgroupSize.x, threadWorkgroupSize.y, threadWorkgroupSize.z,
            maxComputeWorkgroupSize[0], maxComputeWorkgroupSize[1], maxComputeWorkgroupSize[2]));
        return nullptr;
    }

    const std::vector<uint32_t>& spvData = shader.data();
    if (spvData.empty())
    {
        Log::error("Shader data is empty!");
        return nullptr;
    }

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };
    shaderModuleCreateInfo.codeSize = spvData.size() * sizeof(uint32_t);
    shaderModuleCreateInfo.pCode = spvData.data();
    VkResult err = vkCreateShaderModule(this->device, &shaderModuleCreateInfo, this->allocationCallbacks(), &shaderModule);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateShaderModule failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    switch (shader.stage())
    {
    case FV::ShaderStage::Vertex:
    case FV::ShaderStage::Fragment:
    case FV::ShaderStage::Compute:  break;
    default:
        Log::warning("Unsupported shader type!");
    }
    return std::make_shared<ShaderModule>(shared_from_this(), shaderModule, shader);
}

std::shared_ptr<FV::ShaderBindingSet> GraphicsDevice::makeShaderBindingSet(const ShaderBindingSetLayout& layout)
{
    DescriptorPoolID poolID(layout);
    if (poolID.mask)
    {
        uint32_t index = poolID.hash() % NumDescriptorPoolChainBuckets;
        DescriptorPoolChainMap& dpChainMap = descriptorPoolChainMaps[index];

        std::scoped_lock guard(dpChainMap.lock);

        // find matching pool.
        if (auto p = dpChainMap.poolChainMap.find(poolID);
            p != dpChainMap.poolChainMap.end())
        {
            DescriptorPoolChain* chain = p->second;
            FVASSERT_DEBUG(chain->gdevice == this);
            FVASSERT_DEBUG(chain->poolID == poolID);
        }

        // create layout!
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
        };
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
        layoutBindings.reserve(layout.bindings.size());
        for (const ShaderBinding& binding : layout.bindings)
        {
            VkDescriptorSetLayoutBinding  layoutBinding = {};
            layoutBinding.binding = binding.binding;
            layoutBinding.descriptorType = getDescriptorType(binding.type);
            layoutBinding.descriptorCount = binding.arrayLength;

            // input-attachment is for the fragment shader only! (framebuffer load operation)
            if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT &&
                layoutBinding.descriptorCount > 0)
                layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            else
                layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

            //TODO: setup immutable sampler!

            layoutBindings.push_back(layoutBinding);
        }
        layoutCreateInfo.bindingCount = (uint32_t)layoutBindings.size();
        layoutCreateInfo.pBindings = layoutBindings.data();

        VkDescriptorSetLayoutSupport layoutSupport = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT 
        };
        vkGetDescriptorSetLayoutSupport(device, &layoutCreateInfo, &layoutSupport);
        FVASSERT_DEBUG(layoutSupport.supported);

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkResult err = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, allocationCallbacks(), &setLayout);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkCreateDescriptorSetLayout failed: {}",
                                   getVkResultString(err)));
            return nullptr;
        }
        return std::make_shared<ShaderBindingSet>(shared_from_this(), setLayout, poolID, layoutCreateInfo);
    }
    return nullptr;
}

std::shared_ptr<DescriptorSet> GraphicsDevice::makeDescriptorSet(VkDescriptorSetLayout layout, const DescriptorPoolID& poolID)
{
    if (poolID.mask)
    {
        uint32_t index = poolID.hash() % NumDescriptorPoolChainBuckets;
        DescriptorPoolChainMap& dpChainMap = descriptorPoolChainMaps[index];

        std::scoped_lock guard(dpChainMap.lock);

        // find matching pool.
        if (dpChainMap.poolChainMap.contains(poolID) == false)
        {
            // create new pool-chain.
            DescriptorPoolChain* chain = new DescriptorPoolChain(this, poolID);
            dpChainMap.poolChainMap.emplace(poolID, chain);
        }
        DescriptorPoolChain* chain = dpChainMap.poolChainMap.at(poolID);
        FVASSERT_DEBUG(chain->gdevice == this);
        FVASSERT_DEBUG(chain->poolID == poolID);

        DescriptorPoolChain::AllocationInfo info;
        if (chain->allocateDescriptorSet(layout, info))
        {
            FVASSERT_DEBUG(info.descriptorSet);
            FVASSERT_DEBUG(info.descriptorPool);

            return std::make_shared<DescriptorSet>(shared_from_this(), info.descriptorPool, info.descriptorSet);
        }
    }
    return nullptr;
}

void GraphicsDevice::releaseDescriptorSets(DescriptorPool* pool, VkDescriptorSet* sets, uint32_t num)
{
    FVASSERT_DEBUG(pool);

    const DescriptorPoolID& poolID = pool->poolID;
    FVASSERT_DEBUG(poolID.mask);

    constexpr auto cleanupThresholdAllChains = 2000;
    constexpr auto cleanupThreshold = 100;

    uint32_t index = poolID.hash() % NumDescriptorPoolChainBuckets;
    DescriptorPoolChainMap& dpChainMap = descriptorPoolChainMaps[index];

    std::scoped_lock guard(dpChainMap.lock);

    pool->releaseDescriptorSets(sets, num);

    size_t numChainPools = 0;
    if (cleanupThresholdAllChains > 0)
    {
        for (auto& pair : dpChainMap.poolChainMap)
            numChainPools += pair.second->descriptorPoolCount();
    }

    if (numChainPools > cleanupThresholdAllChains)
    {
        std::vector<DescriptorPoolID> emptyChainIDs;
        emptyChainIDs.reserve(dpChainMap.poolChainMap.size());
        for (auto& pair : dpChainMap.poolChainMap)
            emptyChainIDs.push_back(pair.first);
        for (auto& poolID : emptyChainIDs)
        {
            auto it = dpChainMap.poolChainMap.find(poolID);
            DescriptorPoolChain* chain = it->second;
            dpChainMap.poolChainMap.erase(it);
            delete chain;
        }
    }
    else
    {
        auto chain = dpChainMap.poolChainMap[poolID];
        if (chain->descriptorPoolCount() > cleanupThreshold && chain->cleanup() == 0)
        {
            auto it = dpChainMap.poolChainMap.find(poolID);
            DescriptorPoolChain* chain = it->second;
            dpChainMap.poolChainMap.erase(it);
            delete chain;
        }
    }
}

std::shared_ptr<FV::GPUBuffer> GraphicsDevice::makeBuffer(size_t length, GPUBuffer::StorageMode storageMode, CPUCacheMode cpuCacheMode)
{
    if (length == 0) return nullptr;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    _ScopeExit defer = {
        [&] {
            if (buffer != VK_NULL_HANDLE)
                vkDestroyBuffer(device, buffer, allocationCallbacks());
            if (memory != VK_NULL_HANDLE)
                vkFreeMemory(device, memory, allocationCallbacks());
            }
    };

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size = VkDeviceSize(length);
    bufferCreateInfo.usage = 0x1ff;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult err = vkCreateBuffer(device, &bufferCreateInfo, allocationCallbacks(), &buffer);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateBuffer failed: {}", 
                               getVkResultString(err)));
        return nullptr;
    }
    VkMemoryPropertyFlags memProperties;
    switch (storageMode)
    {
    case GPUBuffer::StorageModeShared:
        memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    default:
        memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    }

    VkMemoryDedicatedRequirements dedicatedRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
    };
    VkMemoryRequirements2 memoryRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 
    };
    memoryRequirements.pNext = &dedicatedRequirements;

    VkBufferMemoryRequirementsInfo2 memoryRequirementsInfo = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2
    };
    memoryRequirementsInfo.buffer = buffer;
    vkGetBufferMemoryRequirements2(device, &memoryRequirementsInfo, &memoryRequirements);

    auto& memReqs = memoryRequirements.memoryRequirements;
    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
    };
    memAllocInfo.allocationSize = memReqs.size;
    auto memoryTypeIndex = indexOfMemoryType(memReqs.memoryTypeBits, memProperties);
    if (memoryTypeIndex == uint32_t(-1))
    {
        FVERROR_ABORT("GraphicsDevice error: Unknown memory type!");
    }
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;
    FVASSERT_DEBUG(memAllocInfo.allocationSize >= bufferCreateInfo.size);

    if (dedicatedRequirements.prefersDedicatedAllocation)
    {
        // bind resource to a dedicated allocation.
        VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
            VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
        };
        memoryDedicatedAllocateInfo.buffer = buffer;
        memAllocInfo.pNext = &memoryDedicatedAllocateInfo;

        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }
    else
    {
        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }

    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkAllocateMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    err = vkBindBufferMemory(device, buffer, memory, 0);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkBindBufferMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    VkMemoryType memoryType = this->deviceMemoryTypes[memAllocInfo.memoryTypeIndex];
    auto deviceMemory = std::make_shared<DeviceMemory>(shared_from_this(), memory, memoryType, memAllocInfo.allocationSize);
    memory = nullptr;

    auto bufferObject = std::make_shared<Buffer>(deviceMemory, buffer, bufferCreateInfo);
    buffer = nullptr;

    return std::make_shared<BufferView>(bufferObject);
}

std::shared_ptr<FV::Texture> GraphicsDevice::makeTexture(const TextureDescriptor& desc)
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    _ScopeExit defer = {
    [&] {
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, allocationCallbacks());
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, allocationCallbacks());
        }
    };

    VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    switch (desc.textureType)
    {
    case TextureType1D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
        break;
    case TextureType2D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        break;
    case TextureType3D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
        break;
    case TextureTypeCube:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    default:
        Log::error("GraphicsDevice.makeTexture(): Invalid texture type!");
        FVERROR_ABORT("Invalid texture type!");
        return nullptr;
    }

    if (desc.width < 1 || desc.height < 1 || desc.depth < 1)
    {
        Log::error("Texture dimensions (width, height, depth) value must be greater than or equal to 1.");
        return nullptr;
    }

    imageCreateInfo.arrayLayers = std::max(desc.arrayLength, 1U);
    if (imageCreateInfo.arrayLayers > 1 && imageCreateInfo.imageType == VK_IMAGE_TYPE_2D)
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
    imageCreateInfo.format = getPixelFormat(desc.pixelFormat);
    FVASSERT_DESC_DEBUG(imageCreateInfo.format != VK_FORMAT_UNDEFINED, "Unsupported format!");

    imageCreateInfo.extent.width = desc.width;
    imageCreateInfo.extent.height = desc.height;
    imageCreateInfo.extent.depth = desc.depth;
    imageCreateInfo.mipLevels = desc.mipmapLevels;

    FVASSERT_DESC_DEBUG(desc.sampleCount == 1, "Multisample is not implemented.");
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    if (desc.usage & TextureUsageCopySource)
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (desc.usage & TextureUsageCopyDestination) {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (desc.usage & (TextureUsageShaderRead | desc.usage & TextureUsageSampled))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (desc.usage & (TextureUsageShaderWrite | desc.usage & TextureUsageStorage))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (desc.usage & TextureUsageRenderTarget)
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        if (FV::isDepthFormat(desc.pixelFormat) || FV::isStencilFormat(desc.pixelFormat))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else 
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }

    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // Set initial layout of the image to undefined
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult err = vkCreateImage(device, &imageCreateInfo, allocationCallbacks(), &image);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateImage failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    // Allocate device memory
    VkMemoryDedicatedRequirements dedicatedRequirements = {
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
    };
    VkMemoryRequirements2 memoryRequirements = {
    VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
    };
    memoryRequirements.pNext = &dedicatedRequirements;

    VkImageMemoryRequirementsInfo2 memoryRequirementsInfo = {
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2
    };
    memoryRequirementsInfo.image = image;
    vkGetImageMemoryRequirements2(device, &memoryRequirementsInfo, &memoryRequirements);

    auto& memReqs = memoryRequirements.memoryRequirements;
    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
    };
    VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    memAllocInfo.allocationSize = memReqs.size;

    auto memoryTypeIndex = indexOfMemoryType(memReqs.memoryTypeBits, memProperties);
    if (memoryTypeIndex == uint32_t(-1))
    {
        FVERROR_ABORT("GraphicsDevice error: Unknown memory type!");
    }
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;

    if (dedicatedRequirements.prefersDedicatedAllocation)
    {
        // bind resource to a dedicated allocation.
        VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
            VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
        };
        memoryDedicatedAllocateInfo.image = image;
        memAllocInfo.pNext = &memoryDedicatedAllocateInfo;

        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }
    else
    {
        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkAllocateMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    err = vkBindImageMemory(device, image, memory, 0);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkBindImageMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    VkMemoryType memoryType = this->deviceMemoryTypes[memAllocInfo.memoryTypeIndex];
    auto deviceMemory = std::make_shared<DeviceMemory>(shared_from_this(), memory, memoryType, memAllocInfo.allocationSize);
    memory = nullptr;

    auto imageObject = std::make_shared<Image>(deviceMemory, image, imageCreateInfo);
    image = nullptr;

    if (imageCreateInfo.usage & (VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_STORAGE_BIT |
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
        };
        imageViewCreateInfo.image = imageObject->image;

        switch (desc.textureType)
        {
        case TextureType1D:
            if (desc.arrayLength > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case TextureType2D:
            if (desc.arrayLength > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TextureType3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case TextureTypeCube:
            if (desc.arrayLength > 1)
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            else
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        default:
            FVASSERT_DEBUG("Uknown texture type!");
            return nullptr;
        }

        imageViewCreateInfo.format = imageCreateInfo.format;
        imageViewCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };

        if (FV::isColorFormat(desc.pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
        if (FV::isDepthFormat(desc.pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if (FV::isStencilFormat(desc.pixelFormat))
            imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = imageCreateInfo.arrayLayers;
        imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;

        VkImageView imageView = VK_NULL_HANDLE;
        err = vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks(), &imageView);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkCreateImageView failed: {}",
                                   getVkResultString(err)));
            return nullptr;
        }
        return std::make_shared<ImageView>(imageObject, imageView, imageViewCreateInfo);
    }
    return nullptr;
}

std::shared_ptr<FV::Texture> GraphicsDevice::makeTransientRenderTarget(TextureType textureType, PixelFormat pixelFormat, uint32_t width, uint32_t height, uint32_t depth)
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    _ScopeExit defer = {
    [&] {
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, allocationCallbacks());
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, allocationCallbacks());
        }
    };

    VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    switch (textureType)
    {
    case TextureType1D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_1D;   break;
    case TextureType2D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;   break;
    case TextureType3D:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;   break;
    case TextureTypeCube:
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    default:
        Log::error("GraphicsDevice.makeTexture(): Invalid texture type!");
        FVERROR_ABORT("Invalid texture type!");
        return nullptr;
    }

    if (width < 1 || height < 1 || depth < 1)
    {
        Log::error("Texture dimensions (width, height, depth) value must be greater than or equal to 1.");
        return nullptr;
    }

    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = getPixelFormat(pixelFormat);
    FVASSERT_DESC_DEBUG(imageCreateInfo.format != VK_FORMAT_UNDEFINED, "Unsupported format!");

    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = depth;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    if (FV::isDepthFormat(pixelFormat) || FV::isStencilFormat(pixelFormat))
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else 
    {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }


    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // Set initial layout of the image to undefined
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult err = vkCreateImage(device, &imageCreateInfo, allocationCallbacks(), &image);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateImage failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    // Allocate device memory
    VkMemoryDedicatedRequirements dedicatedRequirements = {
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
    };
    VkMemoryRequirements2 memoryRequirements = {
    VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
    };
    memoryRequirements.pNext = &dedicatedRequirements;

    VkImageMemoryRequirementsInfo2 memoryRequirementsInfo = {
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2
    };
    memoryRequirementsInfo.image = image;
    vkGetImageMemoryRequirements2(device, &memoryRequirementsInfo, &memoryRequirements);

    auto& memReqs = memoryRequirements.memoryRequirements;
    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
    };
    VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    memAllocInfo.allocationSize = memReqs.size;

    auto memoryTypeIndex = indexOfMemoryType(memReqs.memoryTypeBits, memProperties);
    if (memoryTypeIndex == uint32_t(-1))
    {
        FVERROR_ABORT("GraphicsDevice error: Unknown memory type!");
    }
    memAllocInfo.memoryTypeIndex = memoryTypeIndex;

    if (dedicatedRequirements.prefersDedicatedAllocation)
    {
        // bind resource to a dedicated allocation.
        VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
            VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
        };
        memoryDedicatedAllocateInfo.image = image;
        memAllocInfo.pNext = &memoryDedicatedAllocateInfo;

        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }
    else
    {
        err = vkAllocateMemory(device, &memAllocInfo, allocationCallbacks(), &memory);
    }
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkAllocateMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    err = vkBindImageMemory(device, image, memory, 0);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkBindImageMemory failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }

    VkMemoryType memoryType = this->deviceMemoryTypes[memAllocInfo.memoryTypeIndex];
    auto deviceMemory = std::make_shared<DeviceMemory>(shared_from_this(), memory, memoryType, memAllocInfo.allocationSize);
    memory = nullptr;

    auto imageObject = std::make_shared<Image>(deviceMemory, image, imageCreateInfo);
    image = nullptr;

    // create imageView
    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
    };
    imageViewCreateInfo.image = imageObject->image;

    switch (textureType)
    {
    case TextureType1D:
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case TextureType2D:
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case TextureType3D:
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
    case TextureTypeCube:
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    default:
        FVASSERT_DEBUG("Uknown texture type!");
        return nullptr;
    }

    imageViewCreateInfo.format = imageCreateInfo.format;
    imageViewCreateInfo.components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A
    };

    if (FV::isColorFormat(pixelFormat))
        imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (FV::isDepthFormat(pixelFormat))
        imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (FV::isStencilFormat(pixelFormat))
        imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = imageCreateInfo.arrayLayers;
    imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;

    VkImageView imageView = VK_NULL_HANDLE;
    err = vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks(), &imageView);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateImageView failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    return std::make_shared<ImageView>(imageObject, imageView, imageViewCreateInfo);
}

std::shared_ptr<FV::SamplerState> GraphicsDevice::makeSamplerState(const SamplerDescriptor& desc)
{
    VkSamplerCreateInfo createInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    auto filter = [](SamplerMinMagFilter f)->VkFilter
    {
        switch (f)
        {
        case SamplerMinMagFilter::Nearest:
            return VK_FILTER_NEAREST;
        case SamplerMinMagFilter::Linear:
            return VK_FILTER_LINEAR;
        }
        return VK_FILTER_LINEAR;
    };
    auto mipmapMode = [](SamplerMipFilter f)->VkSamplerMipmapMode
    {
        switch (f)
        {
        case SamplerMipFilter::NotMipmapped:
        case SamplerMipFilter::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerMipFilter::Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    };
    auto addressMode = [](SamplerAddressMode m)->VkSamplerAddressMode
    {
        switch (m)
        {
        case SamplerAddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirrorRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToZero:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };
    auto compareOp = [](CompareFunction f)->VkCompareOp
    {
        switch (f)
        {
        case CompareFunctionNever:        return VK_COMPARE_OP_NEVER;
        case CompareFunctionLess:         return VK_COMPARE_OP_LESS;
        case CompareFunctionEqual:        return VK_COMPARE_OP_EQUAL;
        case CompareFunctionLessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunctionGreater:      return VK_COMPARE_OP_GREATER;
        case CompareFunctionNotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunctionGreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareFunctionAlways:       return VK_COMPARE_OP_ALWAYS;
        }
        return VK_COMPARE_OP_NEVER;
    };

    createInfo.minFilter = filter(desc.minFilter);
    createInfo.magFilter = filter(desc.magFilter);
    createInfo.mipmapMode = mipmapMode(desc.mipFilter);
    createInfo.addressModeU = addressMode(desc.addressModeU);
    createInfo.addressModeV = addressMode(desc.addressModeV);
    createInfo.addressModeW = addressMode(desc.addressModeW);
    createInfo.mipLodBias = 0.0f;
    //createInfo.anisotropyEnable = desc.maxAnisotropy > 1 ? VK_TRUE : VK_FALSE;
    createInfo.anisotropyEnable = VK_TRUE;
    createInfo.maxAnisotropy = float(desc.maxAnisotropy);
    createInfo.compareOp = compareOp(desc.compareFunction);
    createInfo.compareEnable = createInfo.compareOp != VK_COMPARE_OP_NEVER;
    createInfo.minLod = desc.lodMinClamp;
    createInfo.maxLod = desc.lodMaxClamp;

    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    createInfo.unnormalizedCoordinates = desc.normalizedCoordinates ? VK_FALSE : VK_TRUE;
    if (createInfo.unnormalizedCoordinates)
    {
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        createInfo.magFilter = createInfo.minFilter;
        createInfo.minLod = 0;
        createInfo.maxLod = 0;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        createInfo.anisotropyEnable = VK_FALSE;
        createInfo.compareEnable = VK_FALSE;
    }

    VkSampler sampler = VK_NULL_HANDLE;
    VkResult err = vkCreateSampler(device, &createInfo, allocationCallbacks(), &sampler);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateSampler failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    return std::make_shared<Sampler>(shared_from_this(), sampler);
}

std::shared_ptr<FV::GPUEvent> GraphicsDevice::makeEvent()
{
    VkSemaphoreCreateInfo createInfo = { 
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkSemaphore semaphore = VK_NULL_HANDLE;

    VkSemaphoreTypeCreateInfoKHR typeCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO 
    };

    if (this->autoIncrementTimelineEvent)
        typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    else
        typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_BINARY;

    typeCreateInfo.initialValue = 0;
    createInfo.pNext = &typeCreateInfo;

    VkResult err = vkCreateSemaphore(device, &createInfo, allocationCallbacks(), &semaphore);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateSemaphore failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    return std::make_shared<Semaphore>(shared_from_this(), semaphore);
}

std::shared_ptr<FV::GPUSemaphore> GraphicsDevice::makeSemaphore()
{
    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore semaphore = VK_NULL_HANDLE;

    VkSemaphoreTypeCreateInfoKHR typeCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO
    };
    typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    typeCreateInfo.initialValue = 0;
    createInfo.pNext = &typeCreateInfo;

    VkResult err = vkCreateSemaphore(device, &createInfo, allocationCallbacks(), &semaphore);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateSemaphore failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    return std::make_shared<TimelineSemaphore>(shared_from_this(), semaphore);
}

std::shared_ptr<FV::RenderPipelineState> GraphicsDevice::makeRenderPipeline(const RenderPipelineDescriptor& desc, PipelineReflection* reflection)
{
	VkResult err = VK_SUCCESS;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

    _ScopeExit _scope_exit = {
        [&] // cleanup resources if function failure.
        {
            if (pipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(device, pipelineLayout, allocationCallbacks());
            if (renderPass != VK_NULL_HANDLE)
                vkDestroyRenderPass(device, renderPass, allocationCallbacks());
            if (pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, pipeline, allocationCallbacks());
        }
    };

	if (desc.vertexFunction)
	{
        FVASSERT_DEBUG(desc.vertexFunction->stage() == ShaderStage::Vertex);
	}
	if (desc.fragmentFunction)
	{
        FVASSERT_DEBUG(desc.fragmentFunction->stage() == ShaderStage::Fragment);
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO 
    };

	// shader stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;

	auto shaderFunctions = { desc.vertexFunction, desc.fragmentFunction };
	shaderStageCreateInfos.reserve(shaderFunctions.size());

	for (auto& fn : shaderFunctions)
	{
		if (fn)
		{
            auto func = std::dynamic_pointer_cast<ShaderFunction>(fn);
            auto& module = func->module;

			VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
            };
			shaderStageCreateInfo.stage = module->stage;
			shaderStageCreateInfo.module = module->module;
			shaderStageCreateInfo.pName = func->functionName.c_str();
			if (func->specializationInfo.mapEntryCount > 0)
				shaderStageCreateInfo.pSpecializationInfo = &func->specializationInfo;

			shaderStageCreateInfos.push_back(shaderStageCreateInfo);
		}
	}
	pipelineCreateInfo.stageCount = (uint32_t)shaderStageCreateInfos.size();
	pipelineCreateInfo.pStages = shaderStageCreateInfos.data();

    pipelineLayout = makePipelineLayout(shaderFunctions, VK_SHADER_STAGE_ALL);
    if (pipelineLayout == VK_NULL_HANDLE)
        return nullptr;
    pipelineCreateInfo.layout = pipelineLayout;

	// vertex input state
	std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
	vertexBindingDescriptions.reserve(desc.vertexDescriptor.layouts.size());
	for (const FV::VertexBufferLayoutDescriptor& bindingDesc : desc.vertexDescriptor.layouts)
	{
		VkVertexInputBindingDescription bind = {};
		bind.binding = bindingDesc.bufferIndex;
		bind.stride = bindingDesc.stride;
		switch (bindingDesc.step)
		{
		case VertexStepRate::Vertex:
			bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; break;
		case VertexStepRate::Instance:
			bind.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE; break;
		default:
			FVASSERT_DEBUG(0);
		}
		vertexBindingDescriptions.push_back(bind);
	}

	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	vertexAttributeDescriptions.reserve(desc.vertexDescriptor.attributes.size());
	for (const FV::VertexAttributeDescriptor& attrDesc : desc.vertexDescriptor.attributes)
	{
		VkVertexInputAttributeDescription attr = {};
		attr.location = attrDesc.location;
		attr.binding = attrDesc.bufferIndex;
		attr.format = getVertexFormat(attrDesc.format);
		attr.offset = attrDesc.offset;
		vertexAttributeDescriptions.push_back(attr);
	}

	VkPipelineVertexInputStateCreateInfo vertexInputState = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };
	vertexInputState.vertexBindingDescriptionCount = (uint32_t)vertexBindingDescriptions.size();
	vertexInputState.pVertexBindingDescriptions = vertexBindingDescriptions.data();
	vertexInputState.vertexAttributeDescriptionCount = (uint32_t)vertexAttributeDescriptions.size();
	vertexInputState.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputState;

	// input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };
    switch (desc.primitiveTopology)
    {
    case PrimitiveType::Point:
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
    case PrimitiveType::Line:
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    case PrimitiveType::LineStrip:
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        break;
    case PrimitiveType::Triangle:
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    case PrimitiveType::TriangleStrip:
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;
    default:
        Log::error("Unknown PrimitiveTopology");
        break;
    }
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;

	// setup viewport
	VkPipelineViewportStateCreateInfo viewportState = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO 
    };
	viewportState.viewportCount = viewportState.scissorCount = 1;
	pipelineCreateInfo.pViewportState = &viewportState;

	// rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationState = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO 
    };
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	if (desc.triangleFillMode == TriangleFillMode::Lines)
	{
		if (this->features().fillModeNonSolid)
			rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		else
			Log::warning("PolygonFillMode not supported for this hardware.");
	}

    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;

	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = desc.rasterizationEnabled ? VK_FALSE : VK_TRUE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.lineWidth = 1.0f;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;

	// setup multisampling
	VkPipelineMultisampleStateCreateInfo multisampleState = { 
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.pSampleMask = nullptr;
	pipelineCreateInfo.pMultisampleState = &multisampleState;

	// setup depth-stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };
    depthStencilState.depthTestEnable = VK_FALSE;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0.0;
    depthStencilState.maxDepthBounds = 1.0;
    depthStencilState.front = VkStencilOpState(VK_STENCIL_OP_KEEP,
                                               VK_STENCIL_OP_KEEP,
                                               VK_STENCIL_OP_KEEP,
                                               VK_COMPARE_OP_ALWAYS,
                                               0xffffffff,
                                               0xffffffff,
                                               0);
    depthStencilState.back = VkStencilOpState(VK_STENCIL_OP_KEEP,
                                              VK_STENCIL_OP_KEEP,
                                              VK_STENCIL_OP_KEEP,
                                              VK_COMPARE_OP_ALWAYS,
                                              0xffffffff,
                                              0xffffffff,
                                              0);
    depthStencilState.stencilTestEnable = VK_FALSE;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;

	// dynamic states
    VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,

        // Provided by VK_VERSION_1_3
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        VK_DYNAMIC_STATE_STENCIL_OP,

        // VK_EXT_extended_dynamic_state
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        // VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, //required: VkPhysicalDeviceExtendedDynamicState3PropertiesEXT.dynamicPrimitiveTopologyUnrestricted

        // VK_EXT_extended_dynamic_state3
        // VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
        // VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
        // VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT
	};
	VkPipelineDynamicStateCreateInfo dynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };
	dynamicState.pDynamicStates = dynamicStateEnables;
	dynamicState.dynamicStateCount = (uint32_t)std::size(dynamicStateEnables);
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// render pass
	VkRenderPassCreateInfo  renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO 
    };
	VkSubpassDescription subpassDesc = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS };
	std::vector<VkAttachmentDescription> attachmentDescriptions;
	std::vector<VkAttachmentReference> subpassInputAttachmentRefs;
	std::vector<VkAttachmentReference> subpassColorAttachmentRefs;
	std::vector<VkAttachmentReference> subpassResolveAttachmentRefs;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
	VkAttachmentReference subpassDepthStencilAttachment = { VK_ATTACHMENT_UNUSED };

	attachmentDescriptions.reserve(desc.colorAttachments.size() + 1);
	subpassColorAttachmentRefs.reserve(desc.colorAttachments.size());
	colorBlendAttachmentStates.reserve(desc.colorAttachments.size());


    auto blendOperation = [](BlendOperation o)->VkBlendOp
    {
        switch (o)
        {
        case BlendOperation::Add:             return VK_BLEND_OP_ADD;
        case BlendOperation::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case BlendOperation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOperation::Min:             return VK_BLEND_OP_MIN;
        case BlendOperation::Max:             return VK_BLEND_OP_MAX;
        }
        FVASSERT_DEBUG(0);
        return VK_BLEND_OP_ADD;
    };
    auto blendFactor = [](BlendFactor f)->VkBlendFactor
    {
        switch (f)
        {
        case BlendFactor::Zero:                     return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One:						return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SourceColor:              return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSourceColor:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::SourceAlpha:              return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSourceAlpha:		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DestinationColor:			return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDestinationColor:	return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::DestinationAlpha:			return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDestinationAlpha:	return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SourceAlphaSaturated:		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case BlendFactor::BlendColor:				return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusBlendColor:		return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case BlendFactor::BlendAlpha:               return VK_BLEND_FACTOR_CONSTANT_ALPHA;
        case BlendFactor::OneMinusBlendAlpha:       return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        }
        FVASSERT_DEBUG(0);
        return VK_BLEND_FACTOR_ZERO;
    };

	uint32_t colorAttachmentRefCount = 0;
	for (const RenderPipelineColorAttachmentDescriptor& attachment : desc.colorAttachments)
	{
		FVASSERT_DEBUG(FV::isColorFormat(attachment.pixelFormat));
		colorAttachmentRefCount = std::max(colorAttachmentRefCount, attachment.index + 1);
	}
    if (colorAttachmentRefCount > this->properties().limits.maxColorAttachments)
    {
        Log::error(std::format(
            "The number of colors attached exceeds the device limit. ({:d} > {:d})",
            colorAttachmentRefCount,
            this->properties().limits.maxColorAttachments));
        return nullptr;
    }
    subpassColorAttachmentRefs.insert(subpassColorAttachmentRefs.end(),
                                      colorAttachmentRefCount,
                                      { VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL });
	for (uint32_t index = 0; index < desc.colorAttachments.size(); ++index)
	{
		const RenderPipelineColorAttachmentDescriptor& attachment = desc.colorAttachments.at(index);

		VkAttachmentDescription attachmentDesc = {};
		attachmentDesc.format = getPixelFormat(attachment.pixelFormat);
		attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesc.loadOp = attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesc.storeOp = attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachmentDescriptions.push_back(attachmentDesc);

        VkPipelineColorBlendAttachmentState blendState = {};
		blendState.blendEnable = attachment.blendState.enabled;
		blendState.srcColorBlendFactor = blendFactor(attachment.blendState.sourceRGBBlendFactor);
		blendState.dstColorBlendFactor = blendFactor(attachment.blendState.destinationRGBBlendFactor);
		blendState.colorBlendOp = blendOperation(attachment.blendState.rgbBlendOperation);
		blendState.srcAlphaBlendFactor = blendFactor(attachment.blendState.sourceAlphaBlendFactor);
		blendState.dstAlphaBlendFactor = blendFactor(attachment.blendState.destinationAlphaBlendFactor);
		blendState.alphaBlendOp = blendOperation(attachment.blendState.alphaBlendOperation);

		blendState.colorWriteMask = 0;
		if (attachment.blendState.writeMask & ColorWriteMaskRed)
            blendState.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
		if (attachment.blendState.writeMask & ColorWriteMaskGreen)
            blendState.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
		if (attachment.blendState.writeMask & ColorWriteMaskBlue)
            blendState.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
		if (attachment.blendState.writeMask & ColorWriteMaskAlpha)
            blendState.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentStates.push_back(blendState);

		FVASSERT_DEBUG(subpassColorAttachmentRefs.size() > attachment.index);
		VkAttachmentReference& attachmentRef = subpassColorAttachmentRefs.at(attachment.index);
		attachmentRef.attachment = index; // index of render-pass-attachment 
		attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	subpassDesc.colorAttachmentCount = (uint32_t)subpassColorAttachmentRefs.size();
	subpassDesc.pColorAttachments = subpassColorAttachmentRefs.data();
	subpassDesc.pResolveAttachments = subpassResolveAttachmentRefs.data();
	subpassDesc.inputAttachmentCount = (uint32_t)subpassInputAttachmentRefs.size();
	subpassDesc.pInputAttachments = subpassInputAttachmentRefs.data();
	if (FV::isDepthFormat(desc.depthStencilAttachmentPixelFormat) ||
        FV::isStencilFormat(desc.depthStencilAttachmentPixelFormat))
	{
        subpassDepthStencilAttachment.attachment = (uint32_t)attachmentDescriptions.size(); // attachment index
        subpassDepthStencilAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // add depth-stencil attachment description
        VkAttachmentDescription attachmentDesc = {};
        attachmentDesc.format = getPixelFormat(desc.depthStencilAttachmentPixelFormat);
        attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attachmentDesc.loadOp = attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.storeOp = attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachmentDescriptions.push_back(attachmentDesc);
		subpassDesc.pDepthStencilAttachment = &subpassDepthStencilAttachment;
	}

	renderPassCreateInfo.attachmentCount = (uint32_t)attachmentDescriptions.size();
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDesc;

	err = vkCreateRenderPass(device, &renderPassCreateInfo, allocationCallbacks(), &renderPass);
	if (err != VK_SUCCESS)
	{
        Log::error(std::format("vkCreateRenderPass failed: {}",
                               getVkResultString(err)));
		return nullptr;
	}
	pipelineCreateInfo.renderPass = renderPass;

	// color blending
	VkPipelineColorBlendStateCreateInfo colorBlendState = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
    };
	colorBlendState.attachmentCount = (uint32_t)colorBlendAttachmentStates.size();
	colorBlendState.pAttachments = colorBlendAttachmentStates.data();
	pipelineCreateInfo.pColorBlendState = &colorBlendState;

	err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, allocationCallbacks(), &pipeline);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateGraphicsPipelines failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
	savePipelineCache();

    if (reflection)
	{
		size_t maxResourceCount = 0;
		size_t maxPushConstantLayoutCount = 0;

		for (auto& fn : shaderFunctions)
		{
			if (fn)
			{
                auto func = std::dynamic_pointer_cast<ShaderFunction>(fn);
                auto& module = func->module;

				maxResourceCount += module->resources.size();
				maxPushConstantLayoutCount += module->pushConstantLayouts.size();

				if (module->stage == VK_SHADER_STAGE_VERTEX_BIT)
				{
                    reflection->inputAttributes.reserve(module->inputAttributes.size());
                    for (const ShaderAttribute& attr : module->inputAttributes)
                    {
                        if (attr.enabled)
                            reflection->inputAttributes.push_back(attr);
                    }
				}
			}
		}

		reflection->resources.clear();
		reflection->pushConstantLayouts.clear();

		reflection->resources.reserve(maxResourceCount);
		reflection->pushConstantLayouts.reserve(maxPushConstantLayoutCount);

		for (auto& fn : shaderFunctions)
		{
			if (fn)
			{
                auto func = std::dynamic_pointer_cast<ShaderFunction>(fn);
                auto& module = func->module;

				uint32_t stageMask = static_cast<uint32_t>(func->stage());
                for (const ShaderResource& res : module->resources)
                {
                    if (!res.enabled)
                        continue;

                    bool exist = false;
                    for (ShaderResource& res2 : reflection->resources)
                    {
                        if (res.set == res2.set && res.binding == res2.binding)
                        {
                            FVASSERT(res.type == res2.type);
                            res2.stages |= stageMask;
                            exist = true;
                            break;
                        }
                    }
                    if (!exist)
                    {
                        ShaderResource res2 = res;
                        res2.stages = stageMask;
                        reflection->resources.push_back(res2);
                    }
                }
				for (const ShaderPushConstantLayout& layout : module->pushConstantLayouts)
				{
					bool exist = false;
					for (ShaderPushConstantLayout& l2 : reflection->pushConstantLayouts)
					{
						if (l2.offset == layout.offset && l2.size == layout.size)
						{
							l2.stages |= stageMask;
                            exist = true;
							break;
						}
					}
					if (!exist)
					{
						ShaderPushConstantLayout l2 = layout;
						l2.stages = stageMask;
						reflection->pushConstantLayouts.push_back(l2);
					}
				}
			}
		}

        reflection->inputAttributes.shrink_to_fit();
		reflection->resources.shrink_to_fit();
		reflection->pushConstantLayouts.shrink_to_fit();
	}

	auto pipelineState = std::make_shared<RenderPipelineState>(shared_from_this(), pipeline, pipelineLayout, renderPass);

    pipelineLayout = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
    pipeline = VK_NULL_HANDLE;

    return pipelineState;
}

std::shared_ptr<FV::ComputePipelineState> GraphicsDevice::makeComputePipeline(const ComputePipelineDescriptor& desc, PipelineReflection* reflection)
{
    VkResult err = VK_SUCCESS;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    _ScopeExit _scope_exit = { 
        [&] // cleanup resources if function failure.
        {
            if (pipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(device, pipelineLayout, allocationCallbacks());
            if (pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(device, pipeline, allocationCallbacks());
        }
    };

    VkComputePipelineCreateInfo pipelineCreateInfo = { 
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
    };

    if (desc.disableOptimization)
        pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    //if (desc.deferCompile)
    //    pipelineCreateInfo.flags |= VK_PIPELINE_CREATE_DEFER_COMPILE_BIT_NVX;

    if (!desc.computeFunction)
        return nullptr; // compute function must be provided.

    auto func = std::dynamic_pointer_cast<ShaderFunction>(desc.computeFunction);
    auto& module = func->module;
    FVASSERT_DEBUG(module->stage == VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
    };
    shaderStageCreateInfo.stage = module->stage;
    shaderStageCreateInfo.module = module->module;
    shaderStageCreateInfo.pName = func->functionName.c_str();
    if (func->specializationInfo.mapEntryCount > 0)
        shaderStageCreateInfo.pSpecializationInfo = &func->specializationInfo;

    pipelineCreateInfo.stage = shaderStageCreateInfo;

    pipelineLayout = makePipelineLayout({ func }, VK_SHADER_STAGE_ALL);
    if (pipelineLayout == VK_NULL_HANDLE)
        return nullptr;

    pipelineCreateInfo.layout = pipelineLayout;
    FVASSERT_DEBUG(pipelineCreateInfo.stage.stage == VK_SHADER_STAGE_COMPUTE_BIT);

    err = vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, allocationCallbacks(), &pipeline);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreateComputePipelines failed: {}",
                               getVkResultString(err)));
        return nullptr;
    }
    savePipelineCache();

    if (reflection)
    {
        reflection->resources = module->resources;
        reflection->resources.shrink_to_fit();
    }

    auto pipelineState = std::make_shared<ComputePipelineState>(shared_from_this(), pipeline, pipelineLayout);

    pipelineLayout = VK_NULL_HANDLE;
    pipeline = VK_NULL_HANDLE;

    return pipelineState;
}

std::shared_ptr<FV::DepthStencilState> GraphicsDevice::makeDepthStencilState(const DepthStencilDescriptor& desc)
{
    auto compareOp = [](CompareFunction f)->VkCompareOp
    {
        switch (f)
        {
        case CompareFunctionNever:        return VK_COMPARE_OP_NEVER;
        case CompareFunctionLess:         return VK_COMPARE_OP_LESS;
        case CompareFunctionEqual:        return VK_COMPARE_OP_EQUAL;
        case CompareFunctionLessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunctionGreater:      return VK_COMPARE_OP_GREATER;
        case CompareFunctionNotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunctionGreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareFunctionAlways:       return VK_COMPARE_OP_ALWAYS;
        }
        FVASSERT_DEBUG(0);
        return VK_COMPARE_OP_ALWAYS;
    };
    auto stencilOp = [](StencilOperation o)->VkStencilOp
    {
        switch (o)
        {
        case StencilOperationKeep:            return VK_STENCIL_OP_KEEP;
        case StencilOperationZero:            return VK_STENCIL_OP_ZERO;
        case StencilOperationReplace:         return VK_STENCIL_OP_REPLACE;
        case StencilOperationIncrementClamp:  return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case StencilOperationDecrementClamp:  return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case StencilOperationInvert:          return VK_STENCIL_OP_INVERT;
        case StencilOperationIncrementWrap:   return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case StencilOperationDecrementWrap:   return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }
        FVASSERT_DEBUG(0);
        return VK_STENCIL_OP_KEEP;
    };
    auto setStencilOpState = [&](VkStencilOpState& state, const StencilDescriptor& stencil)
    {
        state.failOp = stencilOp(stencil.stencilFailureOperation);
        state.passOp = stencilOp(stencil.depthStencilPassOperation);
        state.depthFailOp = stencilOp(stencil.depthFailOperation);
        state.compareOp = compareOp(stencil.stencilCompareFunction);
        state.compareMask = stencil.readMask;
        state.writeMask = stencil.writeMask;
        state.reference = 0; // use dynamic state (VK_DYNAMIC_STATE_STENCIL_REFERENCE)
    };

    auto depthStencilState = std::make_shared<DepthStencilState>(shared_from_this());
    depthStencilState->depthTestEnable = VK_TRUE;
    depthStencilState->depthWriteEnable = desc.depthWriteEnabled;
    depthStencilState->depthCompareOp = compareOp(desc.depthCompareFunction);
    depthStencilState->depthBoundsTestEnable = VK_FALSE;
    setStencilOpState(depthStencilState->front, desc.frontFaceStencil);
    setStencilOpState(depthStencilState->back, desc.backFaceStencil);
    depthStencilState->stencilTestEnable = VK_TRUE;
    depthStencilState->minDepthBounds = 0.0;
    depthStencilState->maxDepthBounds = 1.0;

    if (depthStencilState->front.compareOp == VK_COMPARE_OP_ALWAYS &&
        depthStencilState->front.failOp == VK_STENCIL_OP_KEEP &&
        depthStencilState->front.passOp == VK_STENCIL_OP_KEEP &&
        depthStencilState->front.depthFailOp == VK_STENCIL_OP_KEEP &&
        depthStencilState->back.compareOp == VK_COMPARE_OP_ALWAYS &&
        depthStencilState->back.failOp == VK_STENCIL_OP_KEEP &&
        depthStencilState->back.passOp == VK_STENCIL_OP_KEEP &&
        depthStencilState->back.depthFailOp == VK_STENCIL_OP_KEEP)
    {
        depthStencilState->stencilTestEnable = VK_FALSE;
    }
    if (depthStencilState->depthWriteEnable == VK_FALSE &&
        depthStencilState->depthCompareOp == VK_COMPARE_OP_ALWAYS)
    {
        depthStencilState->depthTestEnable = VK_FALSE;
    }
    return depthStencilState;
}

void GraphicsDevice::loadPipelineCache()
{
	if (this->pipelineCache != VK_NULL_HANDLE)
	{
		vkDestroyPipelineCache(this->device, this->pipelineCache, allocationCallbacks());
		this->pipelineCache = VK_NULL_HANDLE;
	}

	std::vector<unsigned char> buffer;
	VkPipelineCacheCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };

    // load cache from file!
    pipelineCreateInfo.initialDataSize = 0;
    pipelineCreateInfo.pInitialData = nullptr;
	
	VkResult err = vkCreatePipelineCache(this->device, &pipelineCreateInfo, allocationCallbacks(), &this->pipelineCache);
	if (err != VK_SUCCESS)
	{
        Log::error(std::format("vkCreatePipelineCache failed: {}",
                               getVkResultString(err)));
	}
}

void GraphicsDevice::savePipelineCache()
{
	if (this->pipelineCache != VK_NULL_HANDLE)
	{
		std::vector<uint8_t> buffer;
		VkResult err = VK_SUCCESS;
		do
		{
			size_t dataLength = 0;
			err = vkGetPipelineCacheData(this->device, this->pipelineCache, &dataLength, 0);
			if (err != VK_SUCCESS)
				break;

			buffer.resize(dataLength + 4);
			if (dataLength > 0)
			{
				err = vkGetPipelineCacheData(this->device, this->pipelineCache, &dataLength, buffer.data());
				if (err != VK_SUCCESS)
					break;
			}

			buffer.resize(dataLength);
			FVASSERT_DEBUG(buffer.size() == dataLength);

		} while (0);
		if (err == VK_SUCCESS)
		{
            // save data to file!
		}
		else
		{
            Log::error(std::format("vkGetPipelineCacheData failed: {}",
                                   getVkResultString(err)));
		}
	}
	else
	{
		Log::error("VkPipelineCache is NULL");
	}
}

VkPipelineLayout GraphicsDevice::makePipelineLayout(std::initializer_list<std::shared_ptr<FV::ShaderFunction>> functions, VkShaderStageFlags layoutDefaultStageFlags) const
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    auto result = makePipelineLayout(functions, descriptorSetLayouts, layoutDefaultStageFlags);

    for (VkDescriptorSetLayout setLayout : descriptorSetLayouts)
    {
        FVASSERT_DEBUG(setLayout != VK_NULL_HANDLE);
        vkDestroyDescriptorSetLayout(device, setLayout, allocationCallbacks());
    }
    return result;
}

VkPipelineLayout GraphicsDevice::makePipelineLayout(std::initializer_list<std::shared_ptr<FV::ShaderFunction>> functions, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkShaderStageFlags layoutDefaultStageFlags) const
{
    VkResult err = VK_SUCCESS;

    size_t numPushConstantRanges = 0;
    for (auto& fn : functions)
    {
        auto func = std::dynamic_pointer_cast<ShaderFunction>(fn);
        FVASSERT_DEBUG(func);
        auto& module = func->module;

        numPushConstantRanges += module->pushConstantLayouts.size();
    }

    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(numPushConstantRanges);

    size_t maxDescriptorBindings = 0;   // maximum number of descriptor
    uint32_t maxDescriptorSets = 0;     // maximum number of sets

    for (auto& fn : functions)
    {
        auto& module = std::dynamic_pointer_cast<ShaderFunction>(fn)->module;

        for (const FV::ShaderPushConstantLayout& layout : module->pushConstantLayouts)
        {
            if (layout.size > 0)
            {
                VkPushConstantRange range = {};
                range.stageFlags = module->stage;
                range.offset = layout.offset;
                range.size = layout.size;
                pushConstantRanges.push_back(range);
            }
        }

        // calculate max descriptor bindings a set
        if (module->descriptors.empty() == false)
        {
            maxDescriptorSets = std::max(maxDescriptorSets,
                                         module->descriptors.back().set + 1);
            maxDescriptorBindings = std::max(maxDescriptorBindings, module->descriptors.size());
        }
    }

    // setup descriptor layout
    std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
    descriptorBindings.reserve(maxDescriptorBindings);

    for (uint32_t setIndex = 0; setIndex < maxDescriptorSets; ++setIndex)
    {
        descriptorBindings.clear();
        for (auto& fn : functions)
        {
            auto& module = std::dynamic_pointer_cast<ShaderFunction>(fn)->module;

            for (const ShaderDescriptor& desc : module->descriptors)
            {
                if (desc.set > setIndex)
                    break;

                if (desc.set == setIndex)
                {
                    bool newBinding = true;

                    for (VkDescriptorSetLayoutBinding& b : descriptorBindings)
                    {
                        if (b.binding == desc.binding) // exist binding!! (conflict)
                        {
                            newBinding = false;
                            if (b.descriptorType == getDescriptorType(desc.type))
                            {
                                b.descriptorCount = std::max(b.descriptorCount, desc.count);
                                b.stageFlags |= module->stage;
                            }
                            else
                            {
                                Log::error(std::format(
                                    "descriptor binding conflict! (set={:d}, binding={:d})",
                                    setIndex, desc.binding));
                                return VK_NULL_HANDLE;
                            }
                        }
                    }
                    if (newBinding)
                    {
                        VkDescriptorType type = getDescriptorType(desc.type);
                        VkDescriptorSetLayoutBinding binding = {
                            desc.binding,
                            type,
                            desc.count,
                            layoutDefaultStageFlags | module->stage, 
                            nullptr  /* VkSampler* pImmutableSamplers */
                        };
                        descriptorBindings.push_back(binding);
                    }
                }
            }
            
        }
        // create descriptor set (setIndex) layout
        VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        setLayoutCreateInfo.bindingCount = (uint32_t)descriptorBindings.size();
        setLayoutCreateInfo.pBindings = descriptorBindings.data();

        VkDescriptorSetLayoutSupport layoutSupport = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT };
        vkGetDescriptorSetLayoutSupport(device, &setLayoutCreateInfo, &layoutSupport);
        FVASSERT_DEBUG(layoutSupport.supported);

        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        err = vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, allocationCallbacks(), &setLayout);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkCreateDescriptorSetLayout failed: {}",
                                   getVkResultString(err)));
            return VK_NULL_HANDLE;
        }
        descriptorSetLayouts.push_back(setLayout);
        descriptorBindings.clear();
    }
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    err = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, allocationCallbacks(), &pipelineLayout);
    if (err != VK_SUCCESS)
    {
        Log::error(std::format("vkCreatePipelineLayout failed: {}",
                               getVkResultString(err)));
        return VK_NULL_HANDLE;
    }
    return pipelineLayout;
}

void GraphicsDevice::fenceCompletionCallbackThreadProc(std::stop_token stopToken)
{
    constexpr double fenceWaitInterval = 0.002;

    VkResult err = VK_SUCCESS;

    std::vector<VkFence> fences;
    std::vector<FenceCallback> waitingFences;
    std::vector<std::function<void()>> completionHandlers;

    Log::info("Vulkan Queue Completion Helper thread is started.");

    std::unique_lock fenceMutex(fenceCompletionMutex);
    while (stopToken.stop_requested() == false)
    {
        // move callbacks to waitingFences
        waitingFences.insert(waitingFences.end(), pendingFenceCallbacks.begin(), pendingFenceCallbacks.end());
        pendingFenceCallbacks.clear();

        if (waitingFences.empty() == false)
        {
            // condition is unlocked from here.
            fenceMutex.unlock();

            fences.clear();
            fences.reserve(waitingFences.size());
            for (FenceCallback& cb : waitingFences)
                fences.push_back(cb.fence);

            FVASSERT_DEBUG(fences.empty() == false);
            err = vkWaitForFences(device,
                                  (uint32_t)fences.size(), fences.data(),
                                  VK_FALSE, 0);
            fences.clear();
            if (err == VK_SUCCESS)
            {
                std::vector<FenceCallback> waitingFencesCopy;
                waitingFencesCopy.reserve(waitingFences.size());

                // check state for each fences
                for (FenceCallback& cb : waitingFences)
                {
                    if (vkGetFenceStatus(device, cb.fence) == VK_SUCCESS)
                    {
                        fences.push_back(cb.fence);
                        completionHandlers.push_back(cb.completionHandler);
                    }
                    else
                        waitingFencesCopy.push_back(cb); // fence is not ready (unsignaled)
                }
                // save unsignaled fences
                waitingFences.clear();
                waitingFences = std::move(waitingFencesCopy);

                // reset signaled fences
                if (fences.empty() == false)
                {
                    err = vkResetFences(device,
                                        (uint32_t)fences.size(), fences.data());
                    if (err != VK_SUCCESS)
                    {
                        Log::error(std::format("vkResetFences failed: {}",
                                               getVkResultString(err)));
                        // runtime error!
                        //FVASSERT(err == VK_SUCCESS);
                        FVERROR_ABORT("vkResetFences failed");
                    }
                }
            }
            else if (err != VK_TIMEOUT)
            {
                Log::error(std::format("vkResetFences failed: {}",
                                       getVkResultString(err)));
                //FVASSERT(0);
                FVERROR_ABORT("vkResetFences failed");
            }

            if (completionHandlers.empty() == false)
            {
                for (auto& handler : completionHandlers)
                {
                    if (handler)
                        handler();
                }
                completionHandlers.clear();
            }

            // lock condition (requires to reset fences mutually exclusive)
            fenceMutex.lock();
            if (fences.empty() == false)
            {
                reusableFences.insert(reusableFences.end(), fences.begin(), fences.end());
                fences.clear();
            }
            if (err == VK_TIMEOUT)
            {
                if (fenceWaitInterval > 0.0)
                    fenceCompletionCond.wait_for(
                        fenceMutex,
                        std::chrono::duration<double, std::ratio<1, 1>>(fenceWaitInterval),
                        [&] {
                            return stopToken.stop_requested();
                        });
                else
                    std::this_thread::yield();
            }
        }
        else
        {
            fenceCompletionCond.wait(fenceMutex);
        }
    }
    Log::info("Vulkan Queue Completion Helper thread is finished.");
}

void GraphicsDevice::addFenceCompletionHandler(VkFence fence, std::function<void()> op)
{
    FVASSERT_DEBUG(fence != VK_NULL_HANDLE);

    if (op)
    {
        std::unique_lock lock(fenceCompletionMutex);
        FenceCallback cb = { fence, op };
        pendingFenceCallbacks.push_back(cb);
        fenceCompletionCond.notify_all();
    }
}

VkFence GraphicsDevice::getFence()
{
    VkFence fence = VK_NULL_HANDLE;

    if (fence == VK_NULL_HANDLE)
    {
        std::unique_lock lock(fenceCompletionMutex);
        if (reusableFences.empty() == false)
        {
            fence = reusableFences.front();
            reusableFences.erase(reusableFences.begin());
        }
    }
    if (fence == VK_NULL_HANDLE)
    {
        VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkResult err = vkCreateFence(device, &fenceCreateInfo, allocationCallbacks(), &fence);
        if (err != VK_SUCCESS)
        {
            Log::error(std::format("vkCreateFence failed: {}",
                                   getVkResultString(err)));
            FVASSERT(err == VK_SUCCESS);
        }
        std::unique_lock lock(fenceCompletionMutex);
        this->numberOfFences += 1;
        Log::info(std::format("Queue Completion Helper: Num-Fences: {:d}",
                              numberOfFences));
    }
    return fence;
}

#endif //#if FVCORE_ENABLE_VULKAN
