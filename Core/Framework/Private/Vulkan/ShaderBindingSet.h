#pragma once
#include <memory>
#include <vector>
#include "../../ShaderBindingSet.h"

#if FVCORE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#include "BufferView.h"
#include "ImageView.h"
#include "Sampler.h"
#include "DescriptorSet.h"
#include "DescriptorPool.h"

namespace FV::Vulkan {
    class GraphicsDevice;
    class ShaderBindingSet : public FV::ShaderBindingSet {
        const DescriptorPoolID poolID;
    public:
        ShaderBindingSet(std::shared_ptr<GraphicsDevice>,
                         VkDescriptorSetLayout,
                         const DescriptorPoolID&,
                         const VkDescriptorSetLayoutCreateInfo&);
        ~ShaderBindingSet();

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSetLayoutCreateFlags layoutFlags;

        std::shared_ptr<GraphicsDevice> gdevice;

        using DescriptorBinding = DescriptorSet::Binding;
        std::vector<DescriptorBinding> bindings;

        void setBuffer(uint32_t binding, std::shared_ptr<FV::GPUBuffer>, uint64_t, uint64_t) override;
        void setBufferArray(uint32_t binding, uint32_t numBuffers, BufferInfo*) override;
        void setTexture(uint32_t binding, std::shared_ptr<FV::Texture>) override;
        void setTextureArray(uint32_t binding, uint32_t numTextures, std::shared_ptr<FV::Texture>*) override;
        void setSamplerState(uint32_t binding, std::shared_ptr<FV::SamplerState>) override;
        void setSamplerStateArray(uint32_t binding, uint32_t numSamplers, std::shared_ptr<FV::SamplerState>*) override;

        std::shared_ptr<DescriptorSet> makeDescriptorSet() const;

        DescriptorBinding* findDescriptorBinding(uint32_t binding);
    };
}
#endif //#if FVCORE_ENABLE_VULKAN
