// Game.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Game.h"
#include <FVCore.h>
#include <imgui.h>
#include "../Utils/ImGuiFileDialog/ImGuiFileDialog.h"
#include "UIRenderer.h"
#include "VolumeRenderer.h"

using namespace FV;

std::filesystem::path appResourcesRoot;
bool hideUI = false;

struct App : public Application {
    std::shared_ptr<Window> window;
    std::jthread renderThread;
    std::atomic<bool> isVisible;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::shared_ptr<CommandQueue> commandQueue;
    PixelFormat colorFormat;
    PixelFormat depthFormat;

    UIRenderer* uiRenderer;
    VolumeRenderer* volumeRenderer;
    std::vector<std::shared_ptr<Renderer>> renderers;

    void initialize() override {
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "Game.Resources";
        Log::debug("App-Resources: \"{}\"", appResourcesRoot.generic_u8string());

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
                                 [this](const Window::KeyboardEvent& event) {
                                     this->onKeyboardEvent(event);
                                 });
        window->addEventObserver(this,
                                 [this](const Window::WindowEvent& event) {
                                     this->onWindowEvent(event);
                                 });

        window->setContentSize(Size(1280, 720));
        window->activate();

        isVisible = true;
        auto uiRenderer = std::make_shared<UIRenderer>();
        auto volumeRenderer = std::make_shared<VolumeRenderer>();
        this->renderers = {
            volumeRenderer,
            uiRenderer,
        };
        this->volumeRenderer = volumeRenderer.get();
        this->uiRenderer = uiRenderer.get();
        uiRenderer->setWindow(window.get());

        graphicsContext = GraphicsDeviceContext::makeDefault();
        commandQueue = graphicsContext->commandQueue(CommandQueue::Render | CommandQueue::Compute);

        renderThread = std::jthread([this](auto stop) { renderLoop(stop); });
    }

    void finalize() override {
        renderThread.join();

        renderers.clear();
        uiRenderer = nullptr;
        window = nullptr;
        commandQueue = nullptr;
        graphicsContext = nullptr;
    }

    bool openPopupModal = false;
    std::string popupMessage;
    void messageBox(const std::string& mesg) {
        popupMessage = mesg;
        openPopupModal = true;
    }

    void uiLoop(float delta) {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    volumeRenderer->setModel(nullptr);
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

        if (ImGui::Begin("Configuration")) {
            if (ImGui::CollapsingHeader("Camera")) {
                auto pos = camera.position;
                auto dir = camera.dir;
                ImGui::Text(std::format("Position ({:.1f}, {:.1f}, {:.1f})",
                                        pos.x, pos.y, pos.z).c_str());
                ImGui::Text(std::format("Direction ({:.3f}, {:.3f}, {:.3f})",
                                        dir.x, dir.y, dir.z).c_str());
                static auto fov = radianToDegree(camera.fov);
                if (ImGui::SliderFloat("FOV", &fov, 30.0, 160.0, "%.1f")) {
                    camera.fov = degreeToRadian(fov);
                }
                static auto nz = camera.nearZ;
                if (ImGui::SliderFloat("Near", &nz, 0.001f, 1000.0f, "%.3f",
                                       ImGuiSliderFlags_Logarithmic)) {
                    camera.nearZ = nz;

                }
                static auto fz = camera.farZ;
                if (ImGui::SliderFloat("Far", &fz, 1000.0f, 100000.0f, "%.3f",
                                       ImGuiSliderFlags_None)) {
                    camera.farZ = fz;
                }
                static auto mspeed = camera.movementSpeed;
                if (ImGui::SliderFloat("Movement speed", &mspeed,
                                       0.001f, 1000.0, "%.3f",
                                       ImGuiSliderFlags_Logarithmic)) {
                    camera.movementSpeed = mspeed;
                }
                static auto rspeed = camera.rotationSpeed;
                if (ImGui::SliderFloat("Rotation speed", &rspeed,
                                       0.001f, 1.0f, "%.3f",
                                       ImGuiSliderFlags_Logarithmic)) {
                    camera.rotationSpeed = rspeed;
                }
            }
            if (ImGui::CollapsingHeader("Lighting")) {
                static int rotate[2] = {};
                if (ImGui::SliderInt2("Rotate-Roll/Yaw", rotate, 0, 359, nullptr, ImGuiSliderFlags_AlwaysClamp)) {
                    auto qz = Quaternion(Vector3(0, 0, 1), degreeToRadian((float)rotate[0]));
                    auto qy = Quaternion(Vector3(0, 1, 0), degreeToRadian((float)rotate[1]));
                    Vector3 v = { 0, 1, 0 };
                    v = v.applying(qz);
                    v = v.applying(qy);
                    volumeRenderer->lightDir = v;
                }
            }
            static int maxDisplayLevel = volumeRenderer->maxDisplayDepth;
            if (ImGui::SliderInt("Max Display Level", &maxDisplayLevel, 0, 15, nullptr, ImGuiSliderFlags_None)) {
                volumeRenderer->maxDisplayDepth = maxDisplayLevel;
            }
            static int renderScale = int(volumeRenderer->renderScale * 100.0f);
            if (ImGui::SliderInt("Render Scale", &renderScale, 10, 100, nullptr, ImGuiSliderFlags_None)) {
                volumeRenderer->renderScale = float(renderScale) * 0.01f;
            }
        }
        ImGui::End();

        // file dialog 
        if (ImGuiFileDialog::Instance()->Display("OpenVoxelModel")) {
            // action if OK
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                Log::debug("Load model: {}", path);

                auto ifile = std::ifstream(path, std::ios::binary);
                if (ifile) {
                    auto model = std::make_shared<VoxelModel>(nullptr, 0);
                    auto r = model->deserialize(ifile);
                    ifile.close();

                    if (r) {
                        auto nodes = model->numNodes();
                        auto leaf = model->numLeafNodes();

                        Log::debug(
                            enUS_UTF8,
                            "Deserialized result: {}, {:Ld} nodes, {:Ld} leaf-nodes",
                            r, nodes, leaf);

                        volumeRenderer->setModel(model);

                        AABB aabb = model->aabb();
                        auto camPosition = Vector3(aabb.max);
                        auto camTarget = Vector3(aabb.min);

                        this->camera.position = camPosition;
                        this->camera.dir = (camTarget - camPosition).normalized();

                    } else {
                        Log::debug("Deserialization failed.");
                        messageBox("Deserialization failed.");
                    }
                } else {
                    Log::debug("Failed to open file");
                    messageBox("Failed to open file");
                }
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

    struct {
        Vector3 position = { 0, 0, 100 };
        Vector3 dir = { 0, 0, -1 };
        Vector3 up = { 0, 1, 0 };
        float fov = degreeToRadian(80.f);
        float nearZ = 0.01f;
        float farZ = 10000.0f;
        float movementSpeed = 1.0f;
        float rotationSpeed = 0.01f;
    } camera;

    std::mutex mutex;
    std::unordered_set<VirtualKey> pressingKeys;

    void renderLoop(std::stop_token stop) {
        auto swapchain = commandQueue->makeSwapChain(window);
        if (swapchain == nullptr)
            throw std::runtime_error("swapchain creation failed");

        uiRenderer->setSwapChain(swapchain.get());

        for (auto& renderer : this->renderers) {
            renderer->initialize(graphicsContext, swapchain);
        }
        
        auto queue = commandQueue.get();
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
            {
                auto t = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> d = t - timestamp;
                timestamp = t;
                delta = d.count();
            }

            for (auto& renderer : this->renderers) {
                renderer->update(delta);
            }

            std::unique_lock lock(mutex);
            auto movementSpeed = camera.movementSpeed * delta;
            auto dir = camera.dir;
            auto pos = camera.position;
            auto up = camera.up;
            auto left = Vector3::cross(up, dir);

            if (pressingKeys.contains(VirtualKey::W))
                pos += dir * movementSpeed;
            if (pressingKeys.contains(VirtualKey::A))
                pos += left * movementSpeed;
            if (pressingKeys.contains(VirtualKey::D))
                pos -= left * movementSpeed;
            if (pressingKeys.contains(VirtualKey::S))
                pos -= dir * movementSpeed;
            if (pressingKeys.contains(VirtualKey::Q))
                pos += up * movementSpeed;
            if (pressingKeys.contains(VirtualKey::E))
                pos -= up * movementSpeed;

            camera.position = pos;
            auto fov = camera.fov;
            auto nearZ = camera.nearZ;
            auto farZ = camera.farZ;
            lock.unlock();


            do {
                if (this->isVisible.load() == false)
                    break;

                auto rp = swapchain->currentRenderPassDescriptor();

                auto& frontAttachment = rp.colorAttachments.front();
                frontAttachment.clearColor = Color::nonLinearCyan;

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

                    if (1) {
                        auto buffer = queue->makeCommandBuffer();
                        auto encoder = buffer->makeRenderCommandEncoder(rp);
                        encoder->setDepthStencilState(depthStencilState);

                        encoder->endEncoding();
                        buffer->commit();
                    }

                    frontAttachment.loadAction = RenderPassAttachmentDescriptor::LoadActionLoad;

                    auto aspectRatio = float(width) / float(height);
                    auto view = ViewTransform(pos, dir, up);
                    auto projection = ProjectionTransform::perspective(fov, aspectRatio, nearZ, farZ);

                    for (auto& renderer : this->renderers) {
                        renderer->prepareScene(rp, view, projection);
                    }

                    if (!hideUI) {
                        ImGui::NewFrame();
                        uiLoop(delta);
                        ImGui::Render();
                    }

                    for (auto& renderer : this->renderers) {
                        renderer->render(rp, Rect(0, 0, width, height));
                    }
                }

                swapchain->present();
            } while (0);

            //auto t = std::chrono::high_resolution_clock::now();
            //std::chrono::duration<double> d = t - timestamp;

            //auto interval = std::max(frameInterval - d.count(), 0.0);
            //if (interval > 0.0) {
            //    std::this_thread::sleep_for(std::chrono::duration<double>(interval));
            //} else {
                std::this_thread::yield();
            //}
        }
        for (auto& renderer : this->renderers) {
            renderer->finalize();
        }
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

    bool mouseHidden = false;
    void onMouseEvent(const Window::MouseEvent& event) {
        if (event.device == Window::MouseEvent::GenericMouse && event.deviceID == 0) {
            if (event.buttonID == 0) { // left-click
                switch (event.type) {
                case Window::MouseEvent::ButtonDown:
                    window->showMouse(event.deviceID, false);
                    window->lockMouse(event.deviceID, true);
                    mouseHidden = true;
                    break;
                case Window::MouseEvent::ButtonUp:
                    window->showMouse(event.deviceID, true);
                    window->lockMouse(event.deviceID, false);
                    mouseHidden = false;
                    break;
                case Window::MouseEvent::Move:
                    if (mouseHidden) {
                        auto delta = event.delta;
                        //Log::debug("Mouse move: {:.1f}, {:.1f}", delta.x, delta.y);

                        auto up = camera.up;
                        auto dir = camera.dir;
                        auto left = Vector3::cross(dir, up);

                        auto speed = camera.rotationSpeed;
                        auto delta2 = delta * speed;
                        auto dx = Quaternion(up, delta2.x);
                        auto dy = Quaternion(left, delta2.y);
                        auto rot = dx.concatenating(dy).conjugated();

                        auto t = dir.applying(rot);
                        camera.dir = t.normalized();
                    }
                    break;
                }
            }
        }
    }

    void onKeyboardEvent(const Window::KeyboardEvent& event) {
        if (event.deviceID == 0) {
            std::unique_lock lock(mutex, std::defer_lock_t{});
            switch (event.type) {
            case Window::KeyboardEvent::KeyDown:
                lock.lock();
                pressingKeys.insert(event.key);
                break;
            case Window::KeyboardEvent::KeyUp:
                lock.lock();
                if (pressingKeys.contains(VirtualKey::LeftControl) || pressingKeys.contains(VirtualKey::RightControl)) {
                    if (event.key == VirtualKey::U) {
                        hideUI = !hideUI;
                        Log::info("HideUI: {}", hideUI);
                        return;
                    }
                }
                pressingKeys.erase(event.key);
                break;
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
