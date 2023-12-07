#pragma once
#include <memory>
#include <vector>
#include "../../ShaderBindingSet.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "VulkanBufferView.h"
#include "VulkanImageView.h"
#include "VulkanSampler.h"
#include "VulkanDescriptorSet.h"
#include "VulkanDescriptorPool.h"

namespace FV {
    class VulkanGraphicsDevice;
    class VulkanShaderBindingSet : public ShaderBindingSet {
        const VulkanDescriptorPoolID poolID;
    public:
        VulkanShaderBindingSet(std::shared_ptr<VulkanGraphicsDevice>,
                         VkDescriptorSetLayout,
                         const VulkanDescriptorPoolID&,
                         const VkDescriptorSetLayoutCreateInfo&);
        ~VulkanShaderBindingSet();

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSetLayoutCreateFlags layoutFlags;

        std::shared_ptr<VulkanGraphicsDevice> gdevice;

        using DescriptorBinding = VulkanDescriptorSet::Binding;
        std::vector<DescriptorBinding> bindings;

        void setBuffer(uint32_t binding, std::shared_ptr<GPUBuffer>, uint64_t, uint64_t) override;
        void setBufferArray(uint32_t binding, uint32_t numBuffers, BufferInfo*) override;
        void setTexture(uint32_t binding, std::shared_ptr<Texture>) override;
        void setTextureArray(uint32_t binding, uint32_t numTextures, std::shared_ptr<Texture>*) override;
        void setSamplerState(uint32_t binding, std::shared_ptr<SamplerState>) override;
        void setSamplerStateArray(uint32_t binding, uint32_t numSamplers, std::shared_ptr<SamplerState>*) override;

        std::shared_ptr<VulkanDescriptorSet> makeDescriptorSet() const;

        DescriptorBinding* findDescriptorBinding(uint32_t binding);
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
