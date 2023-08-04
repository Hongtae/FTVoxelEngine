// RenderTest.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "RenderTest.h"

#include <memory>
#include <thread>
#include <FVCore.h>

using namespace FV;

class RenderTestApp : public Application
{
public:
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;

    void initialize() override
    {
        Log::debug("OnInitialize");

        graphicsContext = GraphicsDeviceContext::makeDefault();

        window = Window::makeWindow("RenderTest",
                                    Window::StyleGenericWindow,
                                    Window::WindowCallback
        {
            .contentMinSize 
            {
                [](Window*) { return Size(100, 100); } 
            },
            .closeRequest
            {
                [this](Window*)
                {
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

    void finalize() override
    {
        Log::debug("OnFinalize");

        renderThread.join();
        window = nullptr;
        graphicsContext = nullptr;
    }

    void renderLoop(std::stop_token stop)
    {
        auto queue = graphicsContext->renderQueue();
        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        while (stop.stop_requested() == false)
        {
            auto rp = swapchain->currentRenderPassDescriptor();
            rp.colorAttachments.front().clearColor = Color::nonLinearMint;

            auto buffer = queue->makeCommandBuffer();
            auto encoder = buffer->makeRenderCommandEncoder(rp);
            encoder->endEncoding();
            buffer->commit();
            
            swapchain->present();

            std::this_thread::sleep_for(std::chrono::duration<double>(1.0));
        }
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    return RenderTestApp().run();
}
