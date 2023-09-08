// Editor.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Editor.h"
#include <FVCore.h>
#include <imgui.h>
#ifdef _WIN32
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>
#define FVCORE_ENABLE_VULKAN 1
#include <Framework/Private/Vulkan/GraphicsDevice.h>
#include <Framework/Private/Vulkan/CommandQueue.h>
#include <Framework/Private/Vulkan/SwapChain.h>
#include <Framework/Private/Vulkan/RenderCommandEncoder.h>
#include <Framework/Private/Vulkan/ImageView.h>
#endif


#ifdef _WIN32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT(*defaultWndProc)(HWND, UINT, WPARAM, LPARAM) = nullptr;

LRESULT forwardImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (defaultWndProc)
        return (*defaultWndProc)(hWnd, msg, wParam, lParam);

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
#endif

using namespace FV;

class EditorApp : public Application {
public:
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::filesystem::path appResourcesRoot;

    void initUI() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

#ifdef _WIN32
        HWND hWnd = (HWND)window->platformHandle();
        ImGui_ImplWin32_Init(hWnd);

        defaultWndProc = decltype(defaultWndProc)(GetWindowLongPtrW(hWnd, GWLP_WNDPROC));
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)forwardImGuiWndProc);
#endif
    }

    void initialize() override {
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "RenderTest.Resources";
        Log::debug(std::format("App-Resources: \"{}\"", appResourcesRoot.generic_u8string()));

        graphicsContext = GraphicsDeviceContext::makeDefault();

        window = Window::makeWindow(u8"RenderTest",
                                    Window::StyleGenericWindow,
                                    Window::WindowCallback {
            .contentMinSize {
                [](Window*) { return Size(100, 100); }
            },
            .closeRequest {
                [this](Window*) {
                    renderThread.request_stop();
                    terminate(1234);
                    return true;
                }
            },
        });
        window->setContentSize(Size(1024, 768));
        window->activate();

        this->initUI();

        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();
        window = nullptr;
        graphicsContext = nullptr;

#ifdef _WIN32
        ImGui_ImplWin32_Shutdown();
#endif
        ImGui::DestroyContext();
    }

    void uiLoop() {

    }

    void renderLoop(std::stop_token stop) {

        auto queue = graphicsContext->renderQueue();
        auto device = queue->device().get();

        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

#if FVCORE_ENABLE_VULKAN
        struct UIContext {
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        };
        UIContext uiContext = {};
        VkResult err = VK_SUCCESS;

        auto* gdevice = dynamic_cast<FV::Vulkan::GraphicsDevice*>(graphicsContext->device.get());
        if (gdevice == nullptr)
            throw std::runtime_error("Unable to get vulkan device!");

        auto* swapchain2 = dynamic_cast<FV::Vulkan::SwapChain*>(swapchain.get());
        if (swapchain2 == nullptr)
            throw std::runtime_error("Unable to get vulkan swapchain!");

        auto* cqueue = dynamic_cast<FV::Vulkan::CommandQueue*>(swapchain->queue().get());
        if (cqueue == nullptr)
            throw std::runtime_error("Unable to get vulkan command queue!");

        if (true) {
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
            };
            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
            descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            descriptorPoolCreateInfo.maxSets = 1;
            descriptorPoolCreateInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
            descriptorPoolCreateInfo.pPoolSizes = pool_sizes;

            VkResult err = vkCreateDescriptorPool(gdevice->device, &descriptorPoolCreateInfo, nullptr, &uiContext.descriptorPool);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkCreateDescriptorPool error!");

            VkAttachmentDescription attachment = {};
            attachment.format = swapchain2->surfaceFormat.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference color_attachment = {};
            color_attachment.attachment = 0;
            color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment;
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo renderPassCreateInfo = {};
            renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassCreateInfo.attachmentCount = 1;
            renderPassCreateInfo.pAttachments = &attachment;
            renderPassCreateInfo.subpassCount = 1;
            renderPassCreateInfo.pSubpasses = &subpass;
            renderPassCreateInfo.dependencyCount = 1;
            renderPassCreateInfo.pDependencies = &dependency;

            err = vkCreateRenderPass(gdevice->device, &renderPassCreateInfo, nullptr, &uiContext.renderPass);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkCreateRenderPass failed!");

            VkCommandPoolCreateInfo cmdPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            cmdPoolCreateInfo.queueFamilyIndex = cqueue->family->familyIndex;
            cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            err = vkCreateCommandPool(gdevice->device, &cmdPoolCreateInfo, gdevice->allocationCallbacks(), &uiContext.commandPool);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkCreateCommandPool failed");

            VkCommandBufferAllocateInfo  bufferInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            bufferInfo.commandPool = uiContext.commandPool;
            bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            bufferInfo.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(gdevice->device, &bufferInfo, &uiContext.commandBuffer);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkAllocateCommandBuffers failed");

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = gdevice->instance->instance;
            init_info.Device = gdevice->device;
            init_info.PhysicalDevice = gdevice->physicalDevice.device;
            init_info.Queue = cqueue->queue;
            init_info.QueueFamily = cqueue->family->familyIndex;
            init_info.MinImageCount = 2;
            init_info.ImageCount = swapchain->maximumBufferCount();
            init_info.UseDynamicRendering = false;
            init_info.DescriptorPool = uiContext.descriptorPool;
            ImGui_ImplVulkan_Init(&init_info, uiContext.renderPass);

            err = vkResetCommandPool(gdevice->device, uiContext.commandPool, 0);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkResetCommandPool failed");

            VkCommandBufferBeginInfo cbufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            cbufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(uiContext.commandBuffer, &cbufferBeginInfo);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkBeginCommandBuffer failed!");

            ImGui_ImplVulkan_CreateFontsTexture(uiContext.commandBuffer);

            err = vkEndCommandBuffer(uiContext.commandBuffer);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkEndCommandBuffer failed!");

            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &uiContext.commandBuffer;
            err = vkQueueSubmit(cqueue->queue, 1, &submitInfo, VK_NULL_HANDLE);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkQueueSubmit failed!");

            err = vkDeviceWaitIdle(gdevice->device);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkDeviceWaitIdle failed!");

            ImGui_ImplVulkan_DestroyFontUploadObjects();

            VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(gdevice->device, &fenceCreateInfo, gdevice->allocationCallbacks(), &uiContext.fence);
            if (err != VK_SUCCESS)
                throw std::runtime_error("vkCreateFence failed!");
        }
#endif

        PixelFormat depthFormat = PixelFormat::Depth32Float;
        std::shared_ptr<Texture> depthTexture = nullptr;

        auto depthStencilState = device->makeDepthStencilState(
            {
                CompareFunctionLessEqual,
                StencilDescriptor{},
                StencilDescriptor{},
                true
            });

        constexpr auto frameInterval = 1.0 / 60.0;
        auto timestamp = std::chrono::high_resolution_clock::now();
        double delta = 0.0;
        Transform modelTransform = Transform();

        while (stop.stop_requested() == false) {
            auto rp = swapchain->currentRenderPassDescriptor();

            auto& frontAttachment = rp.colorAttachments.front();
            frontAttachment.clearColor = Color::nonLinearCyan;

            uint32_t width = frontAttachment.renderTarget->width();
            uint32_t height = frontAttachment.renderTarget->height();

            if (depthTexture == nullptr ||
                depthTexture->width() != width ||
                depthTexture->height() != height) {
                depthTexture = device->makeTransientRenderTarget(TextureType2D, depthFormat, width, height, 1);
            }
            rp.depthStencilAttachment.renderTarget = depthTexture;
            rp.depthStencilAttachment.loadAction = RenderPassLoadAction::LoadActionClear;
            rp.depthStencilAttachment.storeAction = RenderPassStoreAction::StoreActionDontCare;

            auto buffer = queue->makeCommandBuffer();
            auto encoder = buffer->makeRenderCommandEncoder(rp);
            encoder->setDepthStencilState(depthStencilState);
            encoder->endEncoding();
            buffer->commit();

            if (true) {
#if FVCORE_ENABLE_VULKAN
                ImGui_ImplVulkan_NewFrame();
#endif
#ifdef _WIN32
                ImGui_ImplWin32_NewFrame();
#endif
                ImGui::NewFrame();
                static bool show_demo_window = true;
                ImGui::ShowDemoWindow(&show_demo_window);

                this->uiLoop();

                ImGui::Render();
                ImDrawData* draw_data = ImGui::GetDrawData();
                const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
                if (!is_minimized) {
#if FVCORE_ENABLE_VULKAN
                    err = vkWaitForFences(gdevice->device, 1, &uiContext.fence, VK_TRUE, UINT64_MAX);
                    if (err != VK_SUCCESS)
                        throw std::runtime_error("vkWaitForFences failed.");

                    err = vkResetFences(gdevice->device, 1, &uiContext.fence);
                    if (err != VK_SUCCESS)
                        throw std::runtime_error("vkResetFences failed.");

                    err = vkResetCommandPool(gdevice->device, uiContext.commandPool, 0);
                    if (err != VK_SUCCESS)
                        throw std::runtime_error("vkResetCommandPool failed.");

                    if (uiContext.framebuffer)
                        vkDestroyFramebuffer(gdevice->device, uiContext.framebuffer, gdevice->allocationCallbacks());
                    uiContext.framebuffer = VK_NULL_HANDLE;

                    std::vector<VkImageView> framebufferImageViews;
                    framebufferImageViews.reserve(rp.colorAttachments.size() + 1);
                    for (auto& attachment : rp.colorAttachments) {
                        auto imageView = dynamic_cast<FV::Vulkan::ImageView*>(attachment.renderTarget.get());
                        if (imageView == nullptr)
                            throw std::runtime_error("Unable to get vulkan image view");
                        framebufferImageViews.push_back(imageView->imageView);
                    }

                    VkFramebufferCreateInfo frameBufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
                    frameBufferCreateInfo.renderPass = uiContext.renderPass;
                    frameBufferCreateInfo.attachmentCount = (uint32_t)framebufferImageViews.size();
                    frameBufferCreateInfo.pAttachments = framebufferImageViews.data();
                    frameBufferCreateInfo.width = width;
                    frameBufferCreateInfo.height = height;
                    frameBufferCreateInfo.layers = 1;
                    
                    err = vkCreateFramebuffer(gdevice->device, &frameBufferCreateInfo, gdevice->allocationCallbacks(), &uiContext.framebuffer);
                    if (err != VK_SUCCESS)
                        throw std::runtime_error("vkCreateFramebuffer failed");

                    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
                    commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    vkBeginCommandBuffer(uiContext.commandBuffer, &commandBufferBeginInfo);

                    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
                    renderPassBeginInfo.renderPass = uiContext.renderPass;
                    renderPassBeginInfo.framebuffer = uiContext.framebuffer;
                    renderPassBeginInfo.renderArea.extent.width = width;
                    renderPassBeginInfo.renderArea.extent.height = height;
                    renderPassBeginInfo.clearValueCount = 0;
                    renderPassBeginInfo.pClearValues = nullptr;
                    vkCmdBeginRenderPass(uiContext.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                    ImGui_ImplVulkan_RenderDrawData(draw_data, uiContext.commandBuffer);

                    vkCmdEndRenderPass(uiContext.commandBuffer);
                    vkEndCommandBuffer(uiContext.commandBuffer);

                    VkCommandBufferSubmitInfo cbufferSubmitInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
                    cbufferSubmitInfo.commandBuffer = uiContext.commandBuffer;
                    cbufferSubmitInfo.deviceMask = 0;
                    VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
                    submitInfo.commandBufferInfoCount = 1;
                    submitInfo.pCommandBufferInfos = &cbufferSubmitInfo;

                    err = vkQueueSubmit2(cqueue->queue, 1, &submitInfo, uiContext.fence);
                    if (err != VK_SUCCESS)
                        throw std::runtime_error("vkQueueSubmit2 failed.");
#endif
                }
            }

            swapchain->FV::SwapChain::present();

            auto t = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> d = t - timestamp;
            timestamp = t;
            delta = d.count();

            auto interval = std::max(frameInterval - delta, 0.0);
            if (interval > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(interval));
            } else {
                std::this_thread::yield();
            }
        }

#if FVCORE_ENABLE_VULKAN
        err = vkDeviceWaitIdle(gdevice->device);
        if (err != VK_SUCCESS)
            throw std::runtime_error("vkDeviceWaitIdle failed.");

        ImGui_ImplVulkan_Shutdown();

        vkDestroyFence(gdevice->device, uiContext.fence, gdevice->allocationCallbacks());
        vkFreeCommandBuffers(gdevice->device, uiContext.commandPool, 1, &uiContext.commandBuffer);
        vkDestroyCommandPool(gdevice->device, uiContext.commandPool, gdevice->allocationCallbacks());

        vkDestroyRenderPass(gdevice->device, uiContext.renderPass, gdevice->allocationCallbacks());
        if (uiContext.framebuffer)
            vkDestroyFramebuffer(gdevice->device, uiContext.framebuffer, gdevice->allocationCallbacks());
        vkDestroyDescriptorPool(gdevice->device, uiContext.descriptorPool, gdevice->allocationCallbacks());
#endif
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow) {
    return EditorApp().run();
}
