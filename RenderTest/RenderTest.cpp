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
#include "Model.h"

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

    void renderLoop(std::stop_token stop)
    {
        auto queue = graphicsContext->renderQueue();
        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        // load shader
        auto vsPath = this->appResourcesRoot / "shaders/sample.vert.spv";
        auto fsPath = this->appResourcesRoot / "shaders/sample.frag.spv";
        auto vertexShader = loadShader(vsPath, queue->device().get());
        if (vertexShader.has_value() == false)
            throw std::runtime_error("failed to load shader");
        auto fragmentShader = loadShader(fsPath, queue->device().get());
        if (fragmentShader.has_value() == false)
            throw std::runtime_error("failed to load shader");

        struct PushConstantBuffer
        {
            Matrix4 transform;
            Vector3 lightDir;
            Vector3 color;
        };

        // setup shader binding
        MaterialShaderMap shader = {};
        shader.resourceSemantics = {
            { ShaderBindingLocation{ 0, 1, 0}, MaterialSemantic::BaseColorTexture },
            { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelViewProjectionMatrix},
        };
        //shader.resourceSemantics[MaterialShaderMap::BindingLocation::pushConstant(0)] = ShaderUniformSemantic::ModelViewProjectionMatrix;
        //shader.resourceSemantics[MaterialShaderMap::BindingLocation::pushConstant(0)] = ShaderUniformSemantic::ModelViewProjectionMatrix;
        //shader.resourceSemantics[MaterialShaderMap::BindingLocation::pushConstant(64)] = ShaderUniformSemantic::DirectionalLightDirection;
        //shader.resourceSemantics[MaterialShaderMap::BindingLocation::pushConstant(80)] = ShaderUniformSemantic::DirectionalLightDiffuseColor;
        

        shader.inputAttributeSemantics = {
            { 0, VertexAttributeSemantic::Position },
            { 1, VertexAttributeSemantic::Normal },
            { 2, VertexAttributeSemantic::TextureCoordinates },
        };
        shader.functions = { vertexShader.value(), fragmentShader.value() };

        // load gltf
        auto modelPath = this->appResourcesRoot / "glTF/Duck/glTF-Binary/Duck.glb";
        auto model = loadModel(modelPath, shader, queue.get());
        if (model == nullptr)
            throw std::runtime_error("failed to load glTF");

        SceneState sceneState = {
            .view = ViewFrustum
            {
                ViewTransform(Vector3(0, 0, 500),  Vector3(0, 0, -1), Vector3(0,1,0)),
                ProjectionTransform::perspective(degreeToRadian(90.0f), 1.0, 1.0, 10000.0)
            },
        };

        struct ForEachNode
        {
            SceneNode& node;
            void operator()(std::function<void (SceneNode&)> fn)
            {
                fn(node);

                for (auto& child : node.children)
                    ForEachNode{ child }(fn);
            }
        };
        std::vector<Material*> materials;

        for (auto& scene : model->scenes)
            for (auto& node : scene.nodes)
                ForEachNode{ node }(
                    [&](auto& node)
                    {
                        if (node.mesh.has_value())
                        {
                            for (auto& mesh : node.mesh.value().submeshes)
                                if (mesh.material)
                                    materials.push_back(mesh.material.get());
                        }
                    });

        for (auto& material : materials)
        {
            material->setProperty(ShaderBindingLocation::pushConstant(64),
                                  Vector3(1, 1, 1));
            material->setProperty(ShaderBindingLocation::pushConstant(80),
                                  Vector3(1, 1, 1));
        }

        for (auto& scene : model->scenes)
            for (auto& node : scene.nodes)
                ForEachNode{ node }(
                    [&](auto& node)
                    {
                        if (node.mesh.has_value())
                        {
                            for (auto& mesh : node.mesh.value().submeshes)
                                mesh.updateShadingProperties(&sceneState);
                        }
                    });

        
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
