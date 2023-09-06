#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>

#include "../../GraphicsDevice.h"
#if FVCORE_ENABLE_VULKAN
#include "PhysicalDevice.h"
#include "Vulkan.h"
#include "Extensions.h"
#include "QueueFamily.h"
#include "DescriptorPoolChain.h"
#include "DescriptorPool.h"
#include "DescriptorSet.h"

namespace FV::Vulkan {
    class VulkanInstance;
    class GraphicsDevice : public FV::GraphicsDevice, public std::enable_shared_from_this<GraphicsDevice> {
    public:
        GraphicsDevice(std::shared_ptr<VulkanInstance>,
                       const PhysicalDeviceDescription&,
                       std::vector<std::string> requiredExtensions,
                       std::vector<std::string> optionalExtensions);
        ~GraphicsDevice();

        std::shared_ptr<FV::CommandQueue> makeCommandQueue(uint32_t) override;

        std::shared_ptr<FV::RenderPipelineState> makeRenderPipeline(const RenderPipelineDescriptor&, PipelineReflection*) override;
        std::shared_ptr<FV::ComputePipelineState> makeComputePipeline(const ComputePipelineDescriptor&, PipelineReflection*) override;
        std::shared_ptr<FV::DepthStencilState> makeDepthStencilState(const DepthStencilDescriptor&) override;

        std::shared_ptr<FV::ShaderModule> makeShaderModule(const FV::Shader&) override;
        std::shared_ptr<FV::ShaderBindingSet> makeShaderBindingSet(const ShaderBindingSetLayout&) override;

        std::shared_ptr<FV::GPUBuffer> makeBuffer(size_t, GPUBuffer::StorageMode, CPUCacheMode) override;
        std::shared_ptr<FV::Texture> makeTexture(const TextureDescriptor&) override;
        std::shared_ptr<FV::Texture> makeTransientRenderTarget(TextureType, PixelFormat, uint32_t width, uint32_t height, uint32_t depth) override;

        std::shared_ptr<FV::SamplerState> makeSamplerState(const SamplerDescriptor&) override;
        std::shared_ptr<FV::GPUEvent> makeEvent() override;
        std::shared_ptr<FV::GPUSemaphore> makeSemaphore() override;

        std::string deviceName() const override { return physicalDevice.name(); }

        std::shared_ptr<DescriptorSet> makeDescriptorSet(VkDescriptorSetLayout, const DescriptorPoolID&);
        void releaseDescriptorSets(DescriptorPool*, VkDescriptorSet*, uint32_t);

        void addFenceCompletionHandler(VkFence, std::function<void()>);
        VkFence fence();

        void loadPipelineCache();
        void savePipelineCache();


        std::shared_ptr<VulkanInstance> instance;
        PhysicalDeviceDescription physicalDevice;
        VkDevice device;

        std::vector<QueueFamily*> queueFamilies;
        std::vector<VkMemoryType> deviceMemoryTypes;
        std::vector<VkMemoryHeap> deviceMemoryHeaps;

        // device properties
        VkPhysicalDeviceProperties properties() { return physicalDevice.properties; }
        VkPhysicalDeviceFeatures features() { return physicalDevice.features; }

        VkAllocationCallbacks* allocationCallbacks() const { return instance->allocationCallback; }

        // extensions
        DeviceProc extensionProc; // device procedure

    private:
        VkPipelineLayout makePipelineLayout(std::initializer_list<std::shared_ptr<FV::ShaderFunction>>, VkShaderStageFlags) const;
        VkPipelineLayout makePipelineLayout(std::initializer_list<std::shared_ptr<FV::ShaderFunction>>, std::vector<VkDescriptorSetLayout>&, VkShaderStageFlags) const;

        enum { NumDescriptorPoolChainBuckets = 7 };
        struct DescriptorPoolChainMap {
            std::map<DescriptorPoolID, DescriptorPoolChain*> poolChainMap;
            std::mutex lock;
        } descriptorPoolChainMaps[NumDescriptorPoolChainBuckets];

        uint32_t indexOfMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) {
            for (uint32_t i = 0; i < deviceMemoryTypes.size(); ++i) {
                if ((typeBits & (1U << i)) && (deviceMemoryTypes.at(i).propertyFlags & properties) == properties)
                    return i;
            }
            FVASSERT_DEBUG(0);
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
