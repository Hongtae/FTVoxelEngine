#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>

#include "../../GraphicsDevice.h"
#if FVCORE_ENABLE_VULKAN
#include "VulkanPhysicalDevice.h"
#include "VulkanInstance.h"
#include "VulkanExtensions.h"
#include "VulkanQueueFamily.h"
#include "VulkanDescriptorPoolChain.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"
#include "VulkanDeviceMemory.h"

namespace FV {
    class VulkanInstance;
    class VulkanGraphicsDevice : public GraphicsDevice, public std::enable_shared_from_this<VulkanGraphicsDevice> {
    public:
        VulkanGraphicsDevice(std::shared_ptr<VulkanInstance>,
                       const VulkanPhysicalDeviceDescription&,
                       std::vector<std::string> requiredExtensions,
                       std::vector<std::string> optionalExtensions);
        ~VulkanGraphicsDevice();

        std::shared_ptr<CommandQueue> makeCommandQueue(uint32_t) override;

        std::shared_ptr<RenderPipelineState> makeRenderPipelineState(const RenderPipelineDescriptor&, PipelineReflection*) override;
        std::shared_ptr<ComputePipelineState> makeComputePipelineState(const ComputePipelineDescriptor&, PipelineReflection*) override;
        std::shared_ptr<DepthStencilState> makeDepthStencilState(const DepthStencilDescriptor&) override;

        std::shared_ptr<ShaderModule> makeShaderModule(const Shader&) override;
        std::shared_ptr<ShaderBindingSet> makeShaderBindingSet(const ShaderBindingSetLayout&) override;

        std::shared_ptr<GPUBuffer> makeBuffer(size_t, GPUBuffer::StorageMode, CPUCacheMode) override;
        std::shared_ptr<Texture> makeTexture(const TextureDescriptor&) override;
        std::shared_ptr<Texture> makeTransientRenderTarget(TextureType, PixelFormat, uint32_t width, uint32_t height, uint32_t depth) override;

        std::shared_ptr<SamplerState> makeSamplerState(const SamplerDescriptor&) override;
        std::shared_ptr<GPUEvent> makeEvent() override;
        std::shared_ptr<GPUSemaphore> makeSemaphore() override;

        std::string deviceName() const override { return physicalDevice.name(); }

        std::shared_ptr<VulkanDescriptorSet> makeDescriptorSet(VkDescriptorSetLayout, const VulkanDescriptorPoolID&);
        void releaseDescriptorSets(VulkanDescriptorPool*, VkDescriptorSet*, uint32_t);

        void addFenceCompletionHandler(VkFence, std::function<void()>);
        VkFence fence();

        void loadPipelineCache();
        void savePipelineCache();


        std::shared_ptr<VulkanInstance> instance;
        VulkanPhysicalDeviceDescription physicalDevice;
        VkDevice device;

        std::vector<VulkanQueueFamily*> queueFamilies;

        // device properties
        VkPhysicalDeviceProperties properties() { return physicalDevice.properties; }
        VkPhysicalDeviceFeatures features() { return physicalDevice.features; }

        VkAllocationCallbacks* allocationCallbacks() const { return instance->allocationCallback; }

        // extensions
        VulkanDeviceExtensions extensionProc; // device procedure

    private:
        std::vector<VkMemoryType> deviceMemoryTypes;
        std::vector<VkMemoryHeap> deviceMemoryHeaps;
        std::vector<VulkanMemoryPool*> memoryPools;

        VkPipelineLayout makePipelineLayout(std::initializer_list<std::shared_ptr<ShaderFunction>>, VkShaderStageFlags) const;
        VkPipelineLayout makePipelineLayout(std::initializer_list<std::shared_ptr<ShaderFunction>>, std::vector<VkDescriptorSetLayout>&, VkShaderStageFlags) const;

        enum { NumDescriptorPoolChainBuckets = 7 };
        struct DescriptorPoolChainMap {
            std::map<VulkanDescriptorPoolID, VulkanDescriptorPoolChain*> poolChainMap;
            std::mutex lock;
        } descriptorPoolChainMaps[NumDescriptorPoolChainBuckets];

        uint32_t indexOfMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) {
            for (uint32_t i = 0; i < deviceMemoryTypes.size(); ++i) {
                if ((typeBits & (1U << i)) && (deviceMemoryTypes.at(i).propertyFlags & properties) == properties)
                    return i;
            }
            return uint32_t(-1);
        };

        VkPipelineCache pipelineCache;

        struct FenceCallback {
            VkFence fence;
            std::function<void()> completionHandler;
        };
        std::vector<FenceCallback> pendingFenceCallbacks;
        std::vector<VkFence> reusableFences;
        size_t numberOfFences;

        std::mutex fenceCompletionMutex;
        std::condition_variable fenceCompletionCond;
        std::jthread fenceCompletionThread;
        void fenceCompletionCallbackThreadProc(std::stop_token);

        bool autoIncrementTimelineEvent = false;
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
