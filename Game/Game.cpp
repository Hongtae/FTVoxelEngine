// Game.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Game.h"
#include <FVCore.h>
#include <imgui.h>
#include "../Utils/ImGuiFileDialog/ImGuiFileDialog.h"
#include "UIRenderer.h"

using namespace FV;

std::filesystem::path appResourcesRoot;

struct App : public Application {
    std::shared_ptr<Window> window;
    std::jthread renderThread;
    std::atomic<bool> isVisible;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::shared_ptr<CommandQueue> renderQueue;
    PixelFormat colorFormat;
    PixelFormat depthFormat;

    std::shared_ptr<UIRenderer> uiRenderer;
    std::string popupMessage;

    void initialize() override {
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "Game.Resources";
        Log::debug(std::format("App-Resources: \"{}\"", appResourcesRoot.generic_u8string()));

        window = Window::makeWindow(
            u8"FV Demo",
            Window::StyleGenericWindow,
            Window::WindowCallback {
                .contentMinSize { [](Window*) {
                    return Size(100, 100); 
                    }
                },
                .closeRequest { [this](Window*) {
                    renderThread.request_stop();
                    terminate(1234);
                    return true;
                }
            },
        });

        window->addEventObserver(this,
                                 [this](const Window::MouseEvent& event) {
                                     this->onMouseEvent(event);
                                 });
        window->addEventObserver(this,
                                 [this](const Window::WindowEvent& event) {
                                     this->onWindowEvent(event);
                                 });

        window->setContentSize(Size(1280, 720));
        window->activate();

        isVisible = true;
        uiRenderer = std::make_shared<UIRenderer>();
        uiRenderer->setWindow(window.get());

        graphicsContext = GraphicsDeviceContext::makeDefault();
        renderQueue = graphicsContext->renderQueue();

        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();

        uiRenderer = nullptr;
        window = nullptr;
        renderQueue = nullptr;
        graphicsContext = nullptr;
    }

    void uiLoop(float delta) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {

                }
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    ImGuiFileDialog::Instance()->OpenDialog("OpenVoxelModel", "Choose File", ".vxm", ".");
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit...", "Ctrl+Q")) {

                }
                ImGui::EndMenu();
            }

            if (delta > 0.0f)
                ImGui::Text(std::format(" ({:.2f} FPS)", 1.0 / delta).c_str());
            ImGui::EndMainMenuBar();
        }

        // file dialog 
        if (ImGuiFileDialog::Instance()->Display("OpenVoxelModel")) {
            // action if OK
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();

                Log::debug(std::format("Load model: {}", filePath));
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        // etc..
        static bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);

        if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(popupMessage.c_str());
            if (ImGui::Button("dismiss")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
    }

    void renderLoop(std::stop_token stop) {
        auto swapchain = renderQueue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        uiRenderer->setSwapChain(swapchain.get());
        uiRenderer->initialize(graphicsContext, swapchain);
        
        auto queue = renderQueue.get();
        auto device = queue->device().get();

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

        constexpr auto frameInterval = 1.0 / 60.0;
        auto timestamp = std::chrono::high_resolution_clock::now();
        double delta = 0.0;

        while (stop.stop_requested() == false) {
            auto rp = swapchain->currentRenderPassDescriptor();

            auto& frontAttachment = rp.colorAttachments.front();
            frontAttachment.clearColor = Color::nonLinearBlue;

            uint32_t width = frontAttachment.renderTarget->width();
            uint32_t height = frontAttachment.renderTarget->height();

            if (width > 0 && height > 0) {
                if (depthTexture == nullptr ||
                    depthTexture->width() != width ||
                    depthTexture->height() != height) {
                    depthTexture = device->makeTransientRenderTarget(
                        TextureType2D,
                        depthFormat,
                        width,
                        height,
                        1);
                }
                rp.depthStencilAttachment.renderTarget = depthTexture;
                rp.depthStencilAttachment.loadAction = RenderPassLoadAction::LoadActionClear;
                rp.depthStencilAttachment.storeAction = RenderPassStoreAction::StoreActionDontCare;

                auto buffer = queue->makeCommandBuffer();
                auto encoder = buffer->makeRenderCommandEncoder(rp);
                encoder->setDepthStencilState(depthStencilState);

                encoder->endEncoding();
                buffer->commit();

                frontAttachment.loadAction = RenderPassAttachmentDescriptor::LoadActionLoad;

                uiRenderer->prepareScene(rp, {}, {});
                ImGui::NewFrame();
                uiLoop(delta);
                ImGui::Render();
                uiRenderer->render(rp, Rect(0, 0, width, height));

            }

            swapchain->present();

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
        uiRenderer->finalize();
    }

    void onWindowEvent(const Window::WindowEvent& event) {
        switch (event.type) {
        case Window::WindowEvent::WindowActivated:
        case Window::WindowEvent::WindowShown:
            isVisible = true;
            //Log::info("window is being displayed.");
            break;
        case Window::WindowEvent::WindowHidden:
        case Window::WindowEvent::WindowMinimized:
            isVisible = false;
            //Log::info("Window is hidden");
            break;
        }
    }

    void onMouseEvent(const Window::MouseEvent& event) {
        if (event.device == Window::MouseEvent::GenericMouse && event.deviceID == 0) {
            if (event.buttonID == 0) { // left-click
                switch (event.type) {
                case Window::MouseEvent::ButtonDown:
                    break;
                case Window::MouseEvent::ButtonUp:
                    break;
                case Window::MouseEvent::Move:
                    break;
                }
            }
        }
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow) {
    return App().run();
}
