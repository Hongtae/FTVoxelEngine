// RenderTest.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "RenderTest.h"

#include <memory>
#include <thread>
#include <filesystem>
#include <chrono>
#include <FVCore.h>

#include "../Utils/tinygltf/tiny_gltf.h"
#include "ShaderReflection.h"

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

    std::shared_ptr<ShaderFunction> loadShader(std::filesystem::path path, GraphicsDevice* device)
    {
        if (Shader shader(path); shader.validate())
        {
            Log::info(std::format("Shader description: \"{}\"", path.generic_u8string()));
            printShaderReflection(shader);
            if (auto module = device->makeShaderModule(shader); module)
            {
                auto names = module->functionNames();
                return module->makeFunction(names.front());
            }
        }
        return nullptr;
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
        auto vsPath = this->appResourcesRoot / "shaders/sample.vert.spv";
        auto fsPath = this->appResourcesRoot / "shaders/sample.frag.spv";
        auto vertexShader = loadShader(vsPath, queue->device().get());
        if (vertexShader == nullptr)
            throw std::runtime_error("failed to load shader");
        auto fragmentShader = loadShader(fsPath, queue->device().get());
        if (fragmentShader == nullptr)
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
