// Editor.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Editor.h"
#include <FVCore.h>
#include <imgui.h>
#ifdef _WIN32
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>
#include "../Utils/ImGuiFileDialog/ImGuiFileDialog.h"
#define FVCORE_ENABLE_VULKAN 1
#include <Framework/Private/Vulkan/GraphicsDevice.h>
#include <Framework/Private/Vulkan/CommandQueue.h>
#include <Framework/Private/Vulkan/SwapChain.h>
#include <Framework/Private/Vulkan/RenderCommandEncoder.h>
#include <Framework/Private/Vulkan/ImageView.h>
#endif
#include "Model.h"
#include "ShaderReflection.h"


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

struct ForEachNode {
    SceneNode& node;
    void operator()(std::function<void(SceneNode&)> fn) {
        fn(node);
        for (auto& child : node.children)
            ForEachNode{ child }(fn);
    }
};

class EditorApp : public Application {
public:
    std::shared_ptr<Model> model;
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::shared_ptr<CommandQueue> renderQueue;
    MaterialShaderMap shader = {};
    PixelFormat colorFormat;
    PixelFormat depthFormat;

    std::string popupMessage;

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
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "Editor.Resources";
        Log::debug(std::format("App-Resources: \"{}\"", appResourcesRoot.generic_u8string()));

        window = Window::makeWindow(u8"FV-Editor",
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

        graphicsContext = GraphicsDeviceContext::makeDefault();
        renderQueue = graphicsContext->renderQueue();

        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();
        window = nullptr;

#ifdef _WIN32
        ImGui_ImplWin32_Shutdown();
#endif
        ImGui::DestroyContext();

        renderQueue = nullptr;
        graphicsContext = nullptr;
    }

    void loadModel(std::string path) {
        auto model = ::loadModel(path, shader, renderQueue.get());
        if (model) {
            auto device = renderQueue->device().get();

            // set user-defined property values (lighting)
            Vector3 lightDir = { 1, -1, 1 };
            Vector3 lightColor = { 1, 1, 1 };
            Vector3 ambientColor = { 0.3, 0.3, 0.3 };

            for (auto& scene : model->scenes) {
                for (auto& node : scene.nodes)
                    ForEachNode{ node }(
                        [&](auto& node) {
                            if (node.mesh.has_value()) {
                                auto& mesh = node.mesh.value();
                                if (auto material = mesh.material.get(); material) {
                                    material->attachments.front().format = colorFormat;
                                    material->depthFormat = depthFormat;
                                    material->setProperty(ShaderBindingLocation::pushConstant(64), lightDir);
                                    material->setProperty(ShaderBindingLocation::pushConstant(80), lightColor);
                                    material->setProperty(ShaderBindingLocation::pushConstant(96), ambientColor);
                                }

                                PipelineReflection reflection = {};
                                if (mesh.buildPipelineState(device, &reflection)) {
                                    printPipelineReflection(reflection, Log::Level::Debug);
                                    mesh.initResources(device, Mesh::BufferUsagePolicy::SingleBuffer);
                                } else {
                                    Log::error("Failed to make pipeline descriptor");
                                }
                            }
                        });
            }

            this->model = model;
        }
        else
            messageBox("failed to load glTF");
    }

    void messageBox(const std::string& mesg) {
        popupMessage = mesg;
        ImGui::OpenPopup("Error");
    }

    void uiLoop() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    ImGuiFileDialog::Instance()->OpenDialog("FVEditor_Open3DAsset", "Choose File", ".glb,.gltf", ".");
                }
                
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
                if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "CTRL+X")) {}
                if (ImGui::MenuItem("Copy", "CTRL+C")) {}
                if (ImGui::MenuItem("Paste", "CTRL+V")) {}
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // viewport
        if (ImGui::Begin("Viewport")) {
            ImGui::SeparatorText("Camera");
            static float distance = 100.0f;
            ImGui::SliderFloat("Distance", &distance, 0.01f, 100.0f, "%.2f");
            static float nearZ = 0.1f;
            static float farZ = 10.0f;
            ImGui::DragFloatRange2("Frustum", &nearZ, &farZ, 0.1f, 0.01f, 400.0f,
                                   "Near: %.2f", "Far: %.2f", ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::End();

        // file dialog 
        if (ImGuiFileDialog::Instance()->Display("FVEditor_Open3DAsset")) {
            // action if OK
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();

                loadModel(filePathName);
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }

        // etc..
        static bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);

        if (popupMessage.empty() == false) {
            if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text(popupMessage.c_str());
                if (ImGui::Button("dismiss")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }
        }
    }

    void renderLoop(std::stop_token stop) {

        auto swapchain = renderQueue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        auto queue = renderQueue.get();
        auto device = queue->device().get();

        // load shader
        do {
            // load shader
            auto vsPath = this->appResourcesRoot / "shaders/sample.vert.spv";
            auto fsPath = this->appResourcesRoot / "shaders/sample.frag.spv";
            auto vertexShader = loadShader(vsPath, device);
            if (vertexShader.has_value() == false)
                throw std::runtime_error("failed to load shader");
            auto fragmentShader = loadShader(fsPath, device);
            if (fragmentShader.has_value() == false)
                throw std::runtime_error("failed to load shader");

            struct PushConstantBuffer {
                Matrix4 transform;
                Vector3 lightDir;
                Vector3 color;
            };

            // setup shader binding
            this->shader.resourceSemantics = {
                { ShaderBindingLocation{ 0, 1, 0}, MaterialSemantic::BaseColorTexture },
                { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelViewProjectionMatrix },
                //{ ShaderBindingLocation::pushConstant(64), MaterialSemantic::UserDefined },
                //{ ShaderBindingLocation::pushConstant(80), MaterialSemantic::UserDefined },
                //{ ShaderBindingLocation::pushConstant(96), MaterialSemantic::UserDefined },
            };

            this->shader.inputAttributeSemantics = {
                { 0, VertexAttributeSemantic::Position },
                { 1, VertexAttributeSemantic::Normal },
                { 2, VertexAttributeSemantic::TextureCoordinates },
            };
            this->shader.functions = { vertexShader.value(), fragmentShader.value() };
        } while (0);

        const Vector3 camPosition = Vector3(0, 120, 200);
        const Vector3 camTarget = Vector3(0, 100, 0);
        const float fov = degreeToRadian(80.0f);

        SceneState sceneState = {
            .view = ViewTransform(camPosition,
                              camTarget - camPosition,
                              Vector3(0, 1, 0)),
            .projection = ProjectionTransform::perspective(
                    fov,     // fov
                    1.0,     // aspect ratio
                    1.0,     // near
                    1000.0), // far
            .model = Matrix4::identity
        };

        this->colorFormat = swapchain->pixelFormat();
        this->depthFormat = PixelFormat::Depth32Float;
        std::shared_ptr<Texture> depthTexture = nullptr;

        auto depthStencilState = device->makeDepthStencilState(
            {
                CompareFunctionLessEqual,
                StencilDescriptor{},
                StencilDescriptor{},
                true
            });


#if FVCORE_ENABLE_VULKAN
        // setup ui
        struct UIContext {
            VkFence fence = VK_NULL_HANDLE;
            VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
            VkCommandPool commandPool = VK_NULL_HANDLE;
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        };
        UIContext uiContext = {};
        VkResult err = VK_SUCCESS;

        auto* gdevice = dynamic_cast<FV::Vulkan::GraphicsDevice*>(graphicsContext->device.get());
        if (gdevice == nullptr)
            throw std::runtime_error("Unable to get vulkan device!");

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
            init_info.ImageCount = uint32_t(swapchain->maximumBufferCount());
            init_info.UseDynamicRendering = true;
            init_info.DescriptorPool = uiContext.descriptorPool;
            init_info.ColorAttachmentFormat = FV::Vulkan::getPixelFormat(colorFormat);

            ImGui_ImplVulkan_Init(&init_info, nullptr);

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

            if (model && model->scenes.empty() == false) {
                modelTransform.rotate(Quaternion(Vector3(0, 1, 0), std::numbers::pi * delta * 0.4));
                sceneState.model = modelTransform.matrix4();
                sceneState.projection = ProjectionTransform::perspective(
                    fov, // fov
                    float(width) / float(height),     // aspect ratio
                    1.0,      // near
                    1000.0);  // far

                auto& scene = model->scenes.at(model->defaultSceneIndex);
                for (auto& node : scene.nodes)
                    ForEachNode{ node }(
                        [&](auto& node) {
                            if (node.mesh.has_value()) {
                                node.mesh.value().updateShadingProperties(&sceneState);
                                node.mesh.value().encodeRenderCommand(encoder.get(), 1, 0);
                            }
                        });
            }
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

                    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
                    commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    vkBeginCommandBuffer(uiContext.commandBuffer, &commandBufferBeginInfo);

                    VkRenderingAttachmentInfo colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
                    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                    if (auto imageView = dynamic_cast<FV::Vulkan::ImageView*>(rp.colorAttachments.front().renderTarget.get());
                        imageView) {
                        colorAttachment.imageView = imageView->imageView;
                        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                    }

                    VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
                    renderingInfo.flags = 0;
                    renderingInfo.layerCount = 1;
                    renderingInfo.renderArea = { 0, 0, width, height };
                    renderingInfo.colorAttachmentCount = 1;
                    renderingInfo.pColorAttachments = &colorAttachment;

                    vkCmdBeginRendering(uiContext.commandBuffer, &renderingInfo);
                    ImGui_ImplVulkan_RenderDrawData(draw_data, uiContext.commandBuffer);
                    vkCmdEndRendering(uiContext.commandBuffer);

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
