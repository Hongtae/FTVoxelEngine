// Editor.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Editor.h"
#include <FVCore.h>
#include <imgui.h>

using namespace FV;

class EditorApp : public Application {
public:
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::filesystem::path appResourcesRoot;

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
        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();
        window = nullptr;
        graphicsContext = nullptr;
    }

    void renderLoop(std::stop_token stop) {
        auto queue = graphicsContext->renderQueue();
        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        auto device = queue->device().get();


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
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow) {
    return EditorApp().run();
}
