// RenderTest.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "RenderTest.h"

#include <memory>
#include <thread>
#include <filesystem>
#include <chrono>
#include <FVCore.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../Utils/tinygltf/tiny_gltf.h"

using namespace FV;

class RenderTestApp : public Application
{
public:
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::filesystem::path appResourcesRoot;

    void initialize() override
    {
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "RenderTest.Resources";
        Log::debug(std::format("App-Resources: \"{}\"", appResourcesRoot.generic_u8string()));

        graphicsContext = GraphicsDeviceContext::makeDefault();

        window = Window::makeWindow(u8"RenderTest",
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
        renderThread.join();
        window = nullptr;
        graphicsContext = nullptr;
    }

    bool loadModel(tinygltf::Model& model, std::filesystem::path path)
    {
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool result = loader.LoadBinaryFromFile(&model, &err, &warn, path.generic_string());
        if (warn.empty() == false)
            Log::warning(std::format("glTF warning: {}", warn));
        if (err.empty() == false)
            Log::error(std::format("glTF error: {}", err));

        return result;
    }

    bool loadShader(Shader& shader, std::filesystem::path path)
    {
        return false;
    }

    void renderLoop(std::stop_token stop)
    {
        auto queue = graphicsContext->renderQueue();
        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        // load gltf
        auto modelPath = this->appResourcesRoot / "glTF/Duck/glTF-Binary/Duck.glb";
        tinygltf::Model model;
        if (loadModel(model, modelPath) == false)
            throw std::runtime_error("failed to load glTF");

        // load shader
        Shader vertexShader, fragmentShader;
        auto vsPath = this->appResourcesRoot / "shaders/sample.vert.spv";
        auto fsPath = this->appResourcesRoot / "shaders/sample.frag.spv";
        if (loadShader(vertexShader, vsPath) == false)
            throw std::runtime_error("failed to load shader");
        if (loadShader(fragmentShader, fsPath) == false)
            throw std::runtime_error("failed to load shader");

        constexpr auto frameInterval = 1.0 / 60.0;
        auto timestamp = std::chrono::high_resolution_clock::now();

        while (stop.stop_requested() == false)
        {
            auto rp = swapchain->currentRenderPassDescriptor();
            rp.colorAttachments.front().clearColor = Color::nonLinearMint;

            auto buffer = queue->makeCommandBuffer();
            auto encoder = buffer->makeRenderCommandEncoder(rp);
            encoder->endEncoding();
            buffer->commit();
            
            swapchain->present();

            auto t = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> delta = t - timestamp;
            timestamp = t;

            auto interval = std::max(frameInterval - delta.count(), 0.0);
            if (interval > 0.0)
            {
                std::this_thread::sleep_for(std::chrono::duration<double>(interval));
            }
            else
            {
                std::this_thread::yield();
            }
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
