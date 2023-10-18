#pragma once
#include "Renderer.h"

#ifdef _WIN32
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>
#define FVCORE_ENABLE_VULKAN 1
#include <Framework/Private/Vulkan/GraphicsDevice.h>
#include <Framework/Private/Vulkan/CommandQueue.h>
#include <Framework/Private/Vulkan/SwapChain.h>
#include <Framework/Private/Vulkan/RenderCommandEncoder.h>
#include <Framework/Private/Vulkan/CopyCommandEncoder.h>
#include <Framework/Private/Vulkan/ImageView.h>
#include <Framework/Private/Vulkan/Sampler.h>
#endif

class UIRenderer : public Renderer {
public:
    UIRenderer();
    ~UIRenderer();

    void setWindow(Window*);
    void setSwapChain(SwapChain*);

    void initialize(std::shared_ptr<GraphicsDeviceContext>, std::shared_ptr<SwapChain>) override;
    void finalize() override;

    void update(float delta) override;
    void render(const RenderPassDescriptor&, const Rect&) override;
    void prepareScene(const RenderPassDescriptor&, const ViewTransform&, const ProjectionTransform&) override;

    ImTextureID registerTexture(std::shared_ptr<Texture> texture, std::shared_ptr<SamplerState> sampler = nullptr);
    void unregisterTexture(Texture*);
    ImTextureID textureID(Texture*) const;

private:
    SwapChain* swapchain;
#if FVCORE_ENABLE_VULKAN
    VkFence fence;
    VkCommandBuffer commandBuffer;
    VkCommandPool commandPool;
    VkDescriptorPool descriptorPool;

    std::shared_ptr<FV::Vulkan::CommandQueue> cqueue;
    std::shared_ptr<FV::Vulkan::GraphicsDevice> gdevice;
#endif

    struct UITexture {
        std::shared_ptr<Texture> texture;
        std::shared_ptr<SamplerState> sampler;
        ImTextureID tid;
    };
    std::vector<UITexture> registeredTextures;
    std::shared_ptr<SamplerState> defaultSampler;
};
