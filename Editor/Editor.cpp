// Editor.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Editor.h"
#include <FVCore.h>
#include <imgui.h>
#include "../Utils/ImGuiFileDialog/ImGuiFileDialog.h"
#include "Model.h"
#include "ShaderReflection.h"
#include "Voxel.h"
#include "UIRenderer.h"
#include "MeshRenderer.h"
#include "VolumeRenderer.h"

using namespace FV;

std::filesystem::path appResourcesRoot;

class EditorApp : public Application {
public:
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::shared_ptr<CommandQueue> renderQueue;
    PixelFormat colorFormat;
    PixelFormat depthFormat;

    MeshRenderer* meshRenderer;
    VolumeRenderer* volumeRenderer;
    UIRenderer* uiRenderer;
    std::vector<std::shared_ptr<Renderer>> renderers;

    std::string popupMessage;

    struct {
        Vector3 position = { 0, 0, 100 };
        Vector3 target = Vector3::zero;
        float fov = degreeToRadian(80.f);
        float nearZ = 0.01;
        float farZ = 1000.0;
    } camera;

    std::optional<Point> draggingPosition; // left-click drag

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
        window->addEventObserver(this,
                                 [this](const Window::MouseEvent& event) {
                                     this->onMouseEvent(event);
                                 });

        auto meshRenderer = std::make_shared<MeshRenderer>();
        auto volumeRenderer = std::make_shared<VolumeRenderer>();
        auto uiRenderer = std::make_shared<UIRenderer>();
        this->renderers = {
            meshRenderer, volumeRenderer, uiRenderer
        };
        this->meshRenderer = meshRenderer.get();
        this->volumeRenderer = volumeRenderer.get();
        this->uiRenderer = uiRenderer.get();

        this->uiRenderer->setWindow(window.get());

        graphicsContext = GraphicsDeviceContext::makeDefault();
        renderQueue = graphicsContext->renderQueue();

        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();

        renderers.clear();
        meshRenderer = nullptr;
        volumeRenderer = nullptr;
        uiRenderer = nullptr;

        window = nullptr;
        renderQueue = nullptr;
        graphicsContext = nullptr;
    }

    void onMouseEvent(const Window::MouseEvent& event) {
        if (event.device == Window::MouseEvent::GenericMouse && event.deviceId == 0) {
            if (event.buttonId == 0) { // left-click
                switch (event.type) {
                case Window::MouseEvent::ButtonDown:
                    this->draggingPosition = event.location;
                    break;
                case Window::MouseEvent::ButtonUp:
                    this->draggingPosition.reset();
                    break;
                case Window::MouseEvent::Move:
                    if (this->draggingPosition.has_value()) {
                        auto old = this->draggingPosition.value();
                        auto location = event.location;

                        auto delta = old - location;

                        auto up = Vector3(0, 1, 0);
                        auto dir = (camera.target - camera.position).normalized();
                        auto left = Vector3::cross(dir, up);

                        //auto distance = (camera.position - camera.target).magnitude();
                        auto dx = Quaternion(up, delta.x * 0.01);
                        auto dy = Quaternion(left, delta.y * 0.01);
                        auto rot = dx.concatenating(dy);

                        auto p = camera.position - camera.target;
                        p = p.applying(rot) + camera.target;
                        camera.position = p;

                        this->draggingPosition = location;
                    }
                    break;
                }
            }
        }
    }

    void loadModel(std::filesystem::path path) {
        Log::info(std::format("Loading gltf-model: {}", path.generic_u8string()));
        auto model = meshRenderer->loadModel(path, renderQueue.get(), colorFormat, depthFormat);
        if (model) {
            AABB aabb = meshRenderer->aabb;
            if (aabb.isNull() == false && camera.fov < std::numbers::pi) {
                // adjust camera!
                //auto extent = (aabb.max - aabb.min) * 0.5f;
                //float ext = std::max({ extent.x, extent.y, extent.z });
                float ext = (aabb.max - aabb.min).magnitude() * 0.5f;
                float hfov = camera.fov * 0.5f;
                float distance = ext / tan(hfov) + ext;
                auto offset = aabb.center() - camera.target;
                camera.target += offset;
                camera.position += offset;
                Vector3 dir = (camera.position - camera.target).normalized();
                camera.position = camera.target + dir * distance;
            }
        } else {
            messageBox("failed to load glTF");
        }
    }

    bool openPopupModal = false;
    void messageBox(const std::string& mesg) {
        popupMessage = mesg;
        openPopupModal = true;
    }

    void uiLoop(float delta) {
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
            if (delta > 0.0f)
                ImGui::Text(std::format(" ({:.2f} FPS)", 1.0/delta).c_str());
            ImGui::EndMainMenuBar();
        }

        // viewport
        if (ImGui::Begin("Viewport")) {
            ImGui::SeparatorText("Camera");
            float distance = (camera.position - camera.target).magnitude();
            if (ImGui::SliderFloat("Distance", &distance, 0.01f, 1000.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                auto dir = (camera.position - camera.target).normalized();
                camera.position = camera.target + dir * distance;
            }
            static float nearZ = 0.1f;
            static float farZ = 100.0f;
            ImGui::DragFloatRange2("Frustum", &nearZ, &farZ, 0.1f, 0.01f, 400.0f,
                                   "Near: %.2f", "Far: %.2f", ImGuiSliderFlags_AlwaysClamp);
            extern bool mouseLocked;
            ImGui::Text(std::format("Mouse-Locked: {}", mouseLocked).c_str());
        }
        ImGui::End();

        if (ImGui::Begin("Voxelize")) {

            bool voxelizationInProgress = false;
            ImGui::BeginDisabled(voxelizationInProgress);

            static int depth = 5;
            if (ImGui::SliderInt("Depth Level", &depth, 0, 12, nullptr, ImGuiSliderFlags_None)) {
                // value changed.
            }

            if (ImGui::Button("Convert")) {
                // voxelize..
                auto model = meshRenderer->model.get();
                if (model) {
                    auto triangles = model->triangleList(model->defaultSceneIndex, graphicsContext.get());

                    auto aabbOctree = voxelize(depth, triangles.size(), 0,
                                          [&](uint64_t i)->const Triangle& {
                                              return triangles.at(i);
                                          }, [&](uint64_t* indices, size_t s, const Vector3& p)->uint64_t {
                                              return 0xffffff;
                                          });
                    Log::debug("voxelize done. (test)");
                    if (aabbOctree) {
                        volumeRenderer->aabbOctree = aabbOctree;
                    }
                } else {
                    Log::error("Invalid model");
                    messageBox("Model is not loaded.");
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!voxelizationInProgress);
            if (ImGui::Button("Cancel")) {
                Log::debug("Voxelization cancelled.");
            }
            ImGui::EndDisabled();

            auto aabbOctree = volumeRenderer->aabbOctree.get();
            if (aabbOctree) {
                ImGui::Text(std::format("MaxDepth: {}", aabbOctree->maxDepth).c_str());
                if (ImGui::Button("Make Layer Buffer")) {
                    auto maxDepth = std::min(uint32_t(depth), aabbOctree->maxDepth);
                    Log::info(std::format("make layer buffer. (maxDepth: {})", maxDepth));

                    auto start = std::chrono::high_resolution_clock::now();
                    auto layer = aabbOctree->makeLayer(depth);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration<double>(end - start);
                    Log::info(std::format(std::locale("en_US.UTF-8"),
                        "aabb-octree make layer with depth:{}, nodes:{:Ld} ({:Ld} bytes), elapsed: {}",
                                          maxDepth,
                                          layer->data.size(),
                                          layer->data.size() * sizeof(AABBOctreeLayer::Node),
                                          elapsed.count()));
                }
            }
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

        if (openPopupModal) {
            ImGui::OpenPopup("Error");
            openPopupModal = false;
        }

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
        
        for (auto& renderer : this->renderers) {
            renderer->initialize(renderQueue);
        }

        auto queue = renderQueue.get();
        auto device = queue->device().get();

        this->colorFormat = swapchain->pixelFormat();
        this->depthFormat = PixelFormat::Depth32Float;
        std::shared_ptr<Texture> depthTexture = nullptr;

        constexpr auto frameInterval = 1.0 / 60.0;
        auto timestamp = std::chrono::high_resolution_clock::now();
        double delta = 0.0;
        Transform modelTransform = Transform();

        while (stop.stop_requested() == false) {

            for (auto& renderer : this->renderers) {
                renderer->update(delta);
            }

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
            encoder->endEncoding();
            buffer->commit();

            frontAttachment.loadAction = RenderPassAttachmentDescriptor::LoadActionLoad;

            for (auto& renderer : this->renderers) {
                renderer->prepareScene(rp);
            }

            meshRenderer->view = ViewTransform(
                camera.position, camera.target - camera.position,
                Vector3(0, 1, 0));
            meshRenderer->projection = ProjectionTransform::perspective(
                camera.fov,
                float(width) / float(height),
                camera.nearZ, camera.farZ);

            ImGui::NewFrame();
            this->uiLoop(delta);
            ImGui::Render();

            for (auto& renderer : this->renderers) {
                renderer->render(rp, Rect(0, 0, width, height), renderQueue.get());
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

        for (auto& renderer : this->renderers) {
            renderer->finalize();
        }
    }
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow) {
    return EditorApp().run();
}
