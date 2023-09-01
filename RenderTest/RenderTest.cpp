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

    void renderLoop(std::stop_token stop)
    {
        auto queue = graphicsContext->renderQueue();
        auto swapchain = queue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        auto device = queue->device().get();

        // load shader
        auto vsPath = this->appResourcesRoot / "shaders/sample.vert.spv";
        auto fsPath = this->appResourcesRoot / "shaders/sample.frag.spv";
        auto vertexShader = loadShader(vsPath, device);
        if (vertexShader.has_value() == false)
            throw std::runtime_error("failed to load shader");
        auto fragmentShader = loadShader(fsPath, device);
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
            { ShaderBindingLocation::pushConstant(0), ShaderUniformSemantic::ModelViewProjectionMatrix },
            //{ ShaderBindingLocation::pushConstant(64), MaterialSemantic::UserDefined },
            //{ ShaderBindingLocation::pushConstant(80), MaterialSemantic::UserDefined },
            //{ ShaderBindingLocation::pushConstant(96), MaterialSemantic::UserDefined },
        };

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

        std::vector<Mesh*> meshes;
        std::vector<Material*> materials;

        for (auto& scene : model->scenes)
            for (auto& node : scene.nodes)
                ForEachNode{ node }(
                    [&](auto& node)
                    {
                        if (node.mesh.has_value())
                        {
                            Mesh& mesh = node.mesh.value();
                            meshes.push_back(&mesh);
                            for (auto& submesh : mesh.submeshes)
                                if (submesh.material)
                                    materials.push_back(submesh.material.get());
                        }
                    });

        PixelFormat depthFormat = PixelFormat::Depth32Float;
        std::shared_ptr<Texture> depthTexture = nullptr;

        // set user-defined property values
        for (auto& material : materials)
        {
            material->attachments.front().format = swapchain->pixelFormat();
            material->depthFormat = depthFormat;
            material->setProperty(ShaderBindingLocation::pushConstant(64), Vector3(1, -1, 1));
            material->setProperty(ShaderBindingLocation::pushConstant(80), Vector3(1, 1, 1));
            material->setProperty(ShaderBindingLocation::pushConstant(96), Vector3(0.3, 0.3, 0.3));
        }

        for (auto& mesh : meshes)
        {
            for (int index = 0; index < mesh->submeshes.size(); ++index)
            {
                auto& submesh = mesh->submeshes.at(index);
                PipelineReflection reflection = {};
                if (submesh.buildPipelineState(device, &reflection))
                {
                    printPipelineReflection(reflection, Log::Level::Debug);
                    submesh.initResources(device, Submesh::BufferUsagePolicy::SingleBuffer);
                }
                else
                {
                    Log::error(std::format(
                        "Failed to make pipeline descriptor for mesh:{}, submesh[{:d}]",
                        mesh->name, index));
                }
            }
            mesh->updateShadingProperties(&sceneState);
        }

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

        while (stop.stop_requested() == false)
        {
            auto rp = swapchain->currentRenderPassDescriptor();

            auto& frontAttachment = rp.colorAttachments.front();
            frontAttachment.clearColor = Color::nonLinearMint;

            uint32_t width = frontAttachment.renderTarget->width();
            uint32_t height = frontAttachment.renderTarget->height();

            if (depthTexture == nullptr ||
                depthTexture->width() != width ||
                depthTexture->height() != height)
            {
                depthTexture = device->makeTransientRenderTarget(TextureType2D, depthFormat, width, height, 1);
            }
            rp.depthStencilAttachment.renderTarget = depthTexture;
            rp.depthStencilAttachment.loadAction = RenderPassLoadAction::LoadActionClear;
            rp.depthStencilAttachment.storeAction = RenderPassStoreAction::StoreActionDontCare;

            auto buffer = queue->makeCommandBuffer();
            auto encoder = buffer->makeRenderCommandEncoder(rp);
            encoder->setDepthStencilState(depthStencilState);

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
                    [&](auto& node)
                    {
                        if (node.mesh.has_value())
                        {
                            node.mesh.value().updateShadingProperties(&sceneState);
                            node.mesh.value().encodeRenderCommand(encoder.get(), 1, 0);
                        }
                    });

            encoder->endEncoding();
            buffer->commit();
            
            swapchain->present();

            auto t = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> d = t - timestamp;
            timestamp = t;
            delta = d.count();

            auto interval = std::max(frameInterval - delta, 0.0);
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
    //auto h = LoadLibraryA("C:\\Program Files\\RenderDoc\\RenderDoc.dll");
    return RenderTestApp().run();
}
