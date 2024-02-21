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
#include "VolumeRenderer2.h"

using namespace FV;

std::filesystem::path appResourcesRoot;

class EditorApp : public Application {
public:
    std::atomic<bool> isVisible;
    std::shared_ptr<Window> window;
    std::jthread renderThread;

    std::shared_ptr<GraphicsDeviceContext> graphicsContext;
    std::shared_ptr<CommandQueue> renderQueue;
    PixelFormat colorFormat;
    PixelFormat depthFormat;

    MeshRenderer* meshRenderer;
    VolumeRenderer* volumeRenderer;
    VolumeRenderer2* volumeRenderer2;
    UIRenderer* uiRenderer;
    std::vector<std::shared_ptr<Renderer>> renderers;

    std::string popupMessage;

    struct {
        Vector3 position = { 0, 0, 100 };
        Vector3 target = Vector3::zero;
        float fov = degreeToRadian(80.f);
        float nearZ = 0.01f;
        float farZ = 1000.0f;
    } camera;

    std::optional<Point> draggingPosition; // left-click drag

    void initialize() override {
        appResourcesRoot = environmentPath(EnvironmentPath::AppRoot) / "Editor.Resources";
        Log::debug("App-Resources: \"{}\"", appResourcesRoot.generic_u8string());

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
        window->addEventObserver(this,
                                 [this](const Window::MouseEvent& event) {
                                     this->onMouseEvent(event);
                                 });
        window->addEventObserver(this,
                                 [this](const Window::WindowEvent& event) {
                                     this->onWindowEvent(event);
                                 });

        window->setContentSize(Size(1280, 960));
        window->activate();
        isVisible = true;

        auto meshRenderer = std::make_shared<MeshRenderer>();
        auto volumeRenderer = std::make_shared<VolumeRenderer>();
        auto volumeRenderer2 = std::make_shared<VolumeRenderer2>();
        auto uiRenderer = std::make_shared<UIRenderer>();
        this->renderers = {
            meshRenderer, volumeRenderer, volumeRenderer2, uiRenderer
        };
        this->meshRenderer = meshRenderer.get();
        this->volumeRenderer = volumeRenderer.get();
        this->volumeRenderer2 = volumeRenderer2.get();
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
        volumeRenderer2 = nullptr;
        uiRenderer = nullptr;

        window = nullptr;
        renderQueue = nullptr;
        graphicsContext = nullptr;
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
        Log::info("Loading gltf-model: {}", path.generic_u8string());
        auto model = meshRenderer->loadModel(path, colorFormat, depthFormat);
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

    void voxelize(int depth) {
        auto model = meshRenderer->model.get();
        if (model) {
            auto faces = model->faceList(model->defaultSceneIndex, graphicsContext.get());
            std::unordered_map<Texture*, std::shared_ptr<Image>> cpuAccessibleImages;

            auto aabbOctree = ::voxelize(
                depth, faces.size(), 0,
                [&](uint64_t i)-> Triangle {
                    return {
                        faces.at(i).vertex[0].pos,
                        faces.at(i).vertex[1].pos,
                        faces.at(i).vertex[2].pos
                    };
                },
                [&](uint64_t* indices, size_t s, const Vector3& p)->AABBOctree::Material {
                    Vector4 colors = { 0, 0, 0, 0 };
                    for (int i = 0; i < s; ++i) {
                        uint64_t index = indices[i];
                        MaterialFace face = faces.at(index);

                        auto plane = Plane(face.vertex[0].pos,
                                           face.vertex[1].pos,
                                           face.vertex[2].pos);
                        Vector3 hitpoint = p;
                        if (auto r = plane.rayTest(p, plane.normal()) >= 0.0f) {
                            hitpoint = p + plane.normal() * r;
                        } else if (auto r = plane.rayTest(p, -plane.normal()) >= 0.0f) {
                            hitpoint = p - plane.normal() * r;
                        }
                        auto uvw = Triangle{
                            face.vertex[0].pos,
                            face.vertex[1].pos,
                            face.vertex[2].pos
                        }.barycentric(hitpoint);

                        Vector4 vertexColor =
                            face.vertex[0].color * uvw.x +
                            face.vertex[1].color * uvw.y +
                            face.vertex[2].color * uvw.z;

                        Image* textureImage = nullptr;
                        Vector4 baseColor = { 1, 1, 1, 1 };

                        if (face.material) {
                            if (auto it = face.material->properties.find(MaterialSemantic::BaseColor);
                                it != face.material->properties.end()) {
                                auto floats = it->second.cast<float>();
                                if (floats.size() >= 4) {
                                    baseColor = { floats[0], floats[1], floats[2], floats[3] };
                                } else if (floats.size() == 3) {
                                    baseColor = { floats[0], floats[1], floats[2], 1.0f };
                                }
                            }
                            // find texture
                            std::shared_ptr<Texture> texture = nullptr;
                            if (auto iter = face.material->properties.find(MaterialSemantic::BaseColorTexture);
                                iter != face.material->properties.end()) {
                                texture = std::visit(
                                    [&](auto&& args)->std::shared_ptr<Texture> {
                                        using T = std::decay_t<decltype(args)>;
                                        if constexpr (std::is_same_v<T, MaterialProperty::TextureArray>) {
                                            return args.front();
                                        } else if constexpr (std::is_same_v<T, MaterialProperty::CombinedTextureSamplerArray>) {
                                            return args.front().texture;
                                        }
                                        return nullptr;
                                    }, iter->second.value);
                            }
                            //if (texture == nullptr)
                            //    texture = face.material->defaultTexture;
                            
                            if (texture) {
                                std::shared_ptr<Image> image = nullptr;
                                if (auto iter = cpuAccessibleImages.find(texture.get()); iter != cpuAccessibleImages.end()) {
                                    image = iter->second;
                                } else {
                                    // download image
                                    auto buffer = this->graphicsContext->makeCPUAccessible(texture);
                                    if (buffer) {
                                        image = Image::fromTextureBuffer(
                                            buffer,
                                            texture->width(), texture->height(),
                                            texture->pixelFormat());
                                    }
                                    cpuAccessibleImages[texture.get()] = image;
                                }
                                textureImage = image.get();
                            }
                        }
                        if (textureImage) {
                            Vector2 uv =
                                face.vertex[0].uv * uvw.x +
                                face.vertex[1].uv * uvw.y +
                                face.vertex[2].uv * uvw.z;
                            auto x = (uv.x - floor(uv.x)) * float(textureImage->width - 1);
                            auto y = (uv.y - floor(uv.y)) * float(textureImage->height - 1);
                            auto pixel = textureImage->readPixel(x, y);
                            Vector4 c = { float(pixel.r), float(pixel.g), float(pixel.b), float(pixel.a) };
                            colors += c * baseColor;
                        } else {
                            colors += vertexColor * baseColor;
                        }
                    }
                    colors = colors / float(s);
                    return { Color(colors).rgba8(), 0 };
                });
            Log::debug("voxelize done. (test)");
            if (aabbOctree) {
                volumeRenderer->aabbOctree = aabbOctree;
                volumeRenderer->setOctreeLayer(nullptr);
            }
        } else {
            Log::error("Invalid model");
            messageBox("Model is not loaded.");
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
                    ImGuiFileDialog::Instance()->OpenDialog("FVEditor_Open3DAsset", "Choose File", ".glb,.gltf", ".",
                                                            1, nullptr,
                                                            ImGuiFileDialogFlags_Modal |
                                                            ImGuiFileDialogFlags_ReadOnlyFileNameField |
                                                            ImGuiFileDialogFlags_DisableCreateDirectoryButton);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Import VXM")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ImportVXM", "Choose File", ".vxm", ".",
                                                            1, nullptr,
                                                            ImGuiFileDialogFlags_Modal |
                                                            ImGuiFileDialogFlags_ReadOnlyFileNameField |
                                                            ImGuiFileDialogFlags_DisableCreateDirectoryButton);
                }
                bool enableExport = volumeRenderer2->model().get() != nullptr;
                ImGui::BeginDisabled(!enableExport);
                if (ImGui::MenuItem("Export VXM")) {
                    ImGuiFileDialog::Instance()->OpenDialog("ExportVXM", "Choose File", ".vxm", ".",
                                                            1, nullptr,
                                                            ImGuiFileDialogFlags_ConfirmOverwrite |
                                                            ImGuiFileDialogFlags_Modal);
                }
                ImGui::EndDisabled();

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
            if (ImGui::BeginMenu("Test")) {
                if (ImGui::BeginMenu("Async Test")) {
                    auto currentThreadID = []()-> std::string {
                        std::stringstream ss;
                        ss << std::this_thread::get_id();
                        return ss.str();
                    };
                    if (ImGui::MenuItem("Async test")) {
                        Log::debug("async test - thread:{}", currentThreadID());
                        auto t = async(
                            [&] {
                                Log::debug("async - thread:{}", currentThreadID());
                            });
                        t->wait();
                    }
                    if (ImGui::MenuItem("Await test")) {
                        Log::debug("await test - thread:{}", currentThreadID());
                        auto t = await(
                            [&] {
                                Log::debug("await - thread:{}", currentThreadID());
                                return 1234;
                            });
                        Log::debug("await result: {}, thread:{}", t, currentThreadID());
                    }
                    if (ImGui::MenuItem("Await await test")) {
                        Log::debug("await await test - thread:{}", currentThreadID());
                        auto t = await(
                            [&] {
                                Log::debug("await - 1 - thread:{}", currentThreadID());
                                return await(
                                    [&] {
                                        Log::debug("await - 2 - thread:{}", currentThreadID());
                                        return 1234;
                                    });
                            });
                        Log::debug("await result: {}, thread:{}", t, currentThreadID());
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("VoxelOctree Test")) {
                    if (ImGui::MenuItem("VoxelOctree random update test")) {
                        VoxelModel model(12);
                        int res = model.resolution();

                        struct Location { uint32_t x, y, z; };
                        std::vector<Location> locations;
                        std::random_device r{};
                        std::default_random_engine random(r());
                        std::uniform_int_distribution<uint32_t> uniformDist(0, (res - 1));

                        constexpr uint32_t count = 1U << 24;
                        locations.reserve(count);
                        for (uint32_t i = 0; i < count; ++i) {
                            auto x = uniformDist(random);
                            auto y = uniformDist(random);
                            auto z = uniformDist(random);
                            locations.push_back({ x, y, z });
                        }
                        const Voxel voxel = {};

                        Log::debug("{} items generated. (resolution: {})", count, res);

                        auto t1 = std::chrono::high_resolution_clock::now();
                        for (auto& loc : locations) {
                            model.update(loc.x, loc.y, loc.z, voxel);
                        }
                        auto t2 = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> d = t2 - t1;
                        Log::debug("insert {} items, {} elapsed.", count, d.count());

                        int numLeafNodes = 0;
                        if (auto root = model.root(); root) {
                            numLeafNodes = root->numLeafNodes();
                        }
                        Log::debug("Num-LeafNodes: {}", numLeafNodes);

                        // shuffle
                        std::shuffle(locations.begin(), locations.end(), random);

                        t1 = std::chrono::high_resolution_clock::now();
                        for (auto& loc : locations) {
                            model.erase(loc.x, loc.y, loc.z);
                            if (auto p = model.lookup(loc.x, loc.y, loc.z); p.has_value()) {
                                Log::debug("error!");
                                model.lookup(loc.x, loc.y, loc.z);
                                model.erase(loc.x, loc.y, loc.z);
                            }
                        }
                        t2 = std::chrono::high_resolution_clock::now();
                        d = t2 - t1;
                        Log::debug("erase {} items, {} elapsed.", count, d.count());

                        numLeafNodes = 0;
                        if (auto root = model.root(); root) {
                            numLeafNodes = root->numLeafNodes();
                        }
                        Log::debug("Num-LeafNodes: {}", numLeafNodes);
                        Log::debug("done.");
                    }
                    if (ImGui::MenuItem("VoxelOctree fill test")) {
                        VoxelModel model(8);
                        int res = model.resolution();

                        struct Location { uint32_t x, y, z; };
                        std::vector<Location> locations;

                        std::random_device r{};
                        std::default_random_engine random(r());
                        std::uniform_int_distribution<uint32_t> uniformDist(0, (res - 1));

                        for (uint32_t x = 0; x < res; ++x) {
                            for (uint32_t y = 0; y < res; ++y) {
                                for (uint32_t z = 0; z < res; ++z) {
                                    locations.push_back({ x, y, z });
                                }
                            }
                        }
                        std::shuffle(locations.begin(), locations.end(), random);
                        Log::debug("{} items generated. (resolution: {})", locations.size(), res);
                        const Voxel voxel = {};

                        auto t1 = std::chrono::high_resolution_clock::now();
                        for (auto& loc : locations) {
                            model.update(loc.x, loc.y, loc.z, voxel);
                        }
                        auto t2 = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> d = t2 - t1;
                        Log::debug("insert {} items, {} elapsed.", locations.size(), d.count());

                        int numLeafNodes = 0;
                        if (auto root = model.root(); root) {
                            numLeafNodes = root->numLeafNodes();
                        }
                        Log::debug("Num-LeafNodes: {}", numLeafNodes);

                        std::shuffle(locations.begin(), locations.end(), random);
                        t1 = std::chrono::high_resolution_clock::now();
                        for (auto& loc : locations) {
                            model.erase(loc.x, loc.y, loc.z);
                        }
                        t2 = std::chrono::high_resolution_clock::now();
                        d = t2 - t1;
                        Log::debug("erase {} items, {} elapsed.", locations.size(), d.count());

                        numLeafNodes = 0;
                        if (auto root = model.root(); root) {
                            numLeafNodes = root->numLeafNodes();
                        }
                        Log::debug("Num-LeafNodes: {}", numLeafNodes);
                        Log::debug("done.");
                    }
                    ImGui::Separator();
                    std::filesystem::path path = "D:\\Work\\test.vxm";
                    if (ImGui::MenuItem("Serialize Voxel Model")) {
                        auto model = volumeRenderer2->model();
                        if (model) {
                            auto ofile = std::ofstream(
                                path,
                                std::ios::binary /*|| std::ios::out || std::ios::trunc */);
                            if (ofile) {
                                auto bytes = model->serialize(ofile);
                                ofile.close();
                                auto nodes1 = model->numNodes();
                                auto leaf1 = model->numLeafNodes();
                                Log::debug(
                                    enUS_UTF8,
                                    "Serialized {:Ld} bytes, {:Ld} nodes, {:Ld} leaf-nodes",
                                    bytes, nodes1, leaf1);
                            } else {
                                Log::debug("failed to open file.");
                            }
                        } else {
                            Log::debug("No model loaded.");
                        }
                    }
                    if (ImGui::MenuItem("Deserialize Voxel Model")) {
                        auto ifile = std::ifstream(path,
                                                   std::ios::binary);
                        if (ifile) {
                            auto model2 = std::make_shared<VoxelModel>(nullptr, 0);
                            auto r = model2->deserialize(ifile);
                            ifile.close();

                            auto nodes2 = model2->numNodes();
                            auto leaf2 = model2->numLeafNodes();

                            if (r)
                                Log::debug(
                                    enUS_UTF8,
                                    "Deserialized result: {}, {:Ld} nodes, {:Ld} leaf-nodes",
                                    r, nodes2, leaf2);
                            else
                                Log::debug("Deserialization failed.");
                        } else {
                            Log::debug("Failed to open file");
                        }
                    }
                    ImGui::EndMenu();
                }
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

        if (ImGui::Begin("Lighting")) {
            static int rotate[2] = {};
            if (ImGui::SliderInt2("Rotate-Roll/Yaw", rotate, 0, 359, nullptr, ImGuiSliderFlags_AlwaysClamp)) {
                auto qz = Quaternion(Vector3(0, 0, 1), degreeToRadian((float)rotate[0]));
                auto qy = Quaternion(Vector3(0, 1, 0), degreeToRadian((float)rotate[1]));
                Vector3 v = { 0, 1, 0 };
                v = v.applying(qz);
                v = v.applying(qy);
                meshRenderer->lightDir = v;
                volumeRenderer->lightDir = v;
                volumeRenderer2->lightDir = v;
            }
        }
        ImGui::End();

        if (ImGui::Begin("Voxelize (AABBTree)")) {

            bool voxelizationInProgress = false;
            ImGui::BeginDisabled(voxelizationInProgress);

            static int depth = 5;
            if (ImGui::SliderInt("Depth Level", &depth, 0, 12, nullptr, ImGuiSliderFlags_None)) {
                // value changed.
            }

            if (ImGui::Button("Convert")) {
                // voxelize..
                voxelize(depth);
            }ImGui::SameLine();
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(!voxelizationInProgress);
            if (ImGui::Button("Cancel")) {
                Log::debug("Voxelization cancelled.");
            }
            ImGui::EndDisabled();

            if (depth > 10) {
                ImGui::SameLine();
                ImGui::Text("(UNSAFE)");
            }

            auto aabbOctree = volumeRenderer->aabbOctree.get();
            if (aabbOctree) {

                int maxDepth = aabbOctree->maxDepth;
                float bestFitDepth = volumeRenderer->bestFitDepth();
                ImGui::Text(std::format("MaxDepth: {}, BestFit: {:.1f}", maxDepth, bestFitDepth).c_str());

                static bool autoFit = false;
                ImGui::SameLine();
                if (ImGui::Checkbox("Auto Fit", &autoFit)) {}
                ImGui::BeginDisabled(autoFit);

                static int layerDepth = 0;
                if (layerDepth > maxDepth) layerDepth = maxDepth;

                bool valueChanged = false;
                if (ImGui::SliderInt("Layer Depth", &layerDepth, 0, maxDepth, nullptr, ImGuiSliderFlags_None)) {
                    valueChanged = true;
                } else if (autoFit) {
                    int bf = std::min((int)std::lround(bestFitDepth), maxDepth);
                    if (layerDepth != bf) {
                        layerDepth = bf;
                        valueChanged = true;
                    }
                }
                if (volumeRenderer->layer() == nullptr) {
                    valueChanged = true;
                }

                if (valueChanged) {
                    auto depth = std::min(uint32_t(layerDepth), aabbOctree->maxDepth);
                    Log::info("make layer buffer. (maxDepth: {})", depth);

                    auto start = std::chrono::high_resolution_clock::now();
                    auto layer = aabbOctree->makeLayer(depth);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration<double>(end - start);
                    Log::info(enUS_UTF8,
                              "aabb-octree make layer with depth:{}, nodes:{:Ld} ({:Ld} bytes), elapsed: {}",
                              maxDepth,
                              layer->data.size(),
                              layer->data.size() * sizeof(AABBOctreeLayer::Node),
                              elapsed.count());
                    volumeRenderer->setOctreeLayer(layer);
                }
                ImGui::EndDisabled();
            }

            auto texture = volumeRenderer->texture;
            ImGui::Text(std::format("Volume Image ({} x {})",
                                    texture->width(), texture->height()).c_str());
            ImGui::Image(uiRenderer->textureID(texture.get()), ImVec2(
                texture->width(), texture->height()));
        }
        ImGui::End();

        if (ImGui::Begin("Voxelize (Layered)")) {

            static int depth = 5;
            if (ImGui::SliderInt("Depth Level", &depth, 0, 15, nullptr, ImGuiSliderFlags_None)) {
                // value changed.
            }
            if (depth > 12) {
                ImGui::SameLine();
                ImGui::Text("(UNSAFE)");
            }

            if (ImGui::Button("Convert-2")) {
                auto model = meshRenderer->model.get();
                if (model) {
                    auto builder = model->voxelBuilder(model->defaultSceneIndex, graphicsContext.get());
                    if (builder) {
                        auto start = std::chrono::high_resolution_clock::now();
                        auto voxelModel = std::make_shared<VoxelModel>(builder.get(), depth);
                        auto end = std::chrono::high_resolution_clock::now();
                        auto elapsed = std::chrono::duration<double>(end - start);

                        if (voxelModel) {
                            uint64_t numNodes = 0;
                            uint64_t numLeafNodes = 0;
                            if (auto root = voxelModel->root(); root) {
                                numNodes = root->numDescendants();
                                numLeafNodes = root->numLeafNodes();
                            }
                            Log::info(
                                enUS_UTF8,
                                "VoxelModel depth:{}, nodes: {:Ld}, leaf-nodes: {:Ld}, elapsed:{}",
                                voxelModel->depth(), numNodes, numLeafNodes, elapsed.count());
                            volumeRenderer2->setModel(voxelModel);
                        } else {
                            Log::info("No output.");
                        }

                    } else {
                        Log::error("Invalid model.");
                        messageBox("Model Error");
                    }
                } else {
                    Log::error("Invalid model");
                    messageBox("Model is not loaded.");
                }
            }
            ImGui::SameLine();

            auto texture = volumeRenderer2->renderTarget;
            ImGui::Text(std::format("Volume Image ({} x {})",
                                    texture->width(), texture->height()).c_str());
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                volumeRenderer2->setModel(nullptr);
            }
            ImGui::Image(uiRenderer->textureID(texture.get()), ImVec2(
                texture->width(), texture->height()));
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
        if (ImGuiFileDialog::Instance()->Display("ImportVXM")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();

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

                        volumeRenderer2->setModel(model);
                    } else {
                        Log::debug("Deserialization failed.");
                        messageBox("Deserialization failed.");
                    }
                } else {
                    Log::debug("Failed to open file");
                    messageBox("Failed to open file");
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGuiFileDialog::Instance()->Display("ExportVXM")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();

                Log::debug("Export vxm to {}", path);

                auto model = volumeRenderer2->model();
                if (model) {
                    auto ofile = std::ofstream(
                        path,
                        std::ios::binary /*|| std::ios::out || std::ios::trunc */);
                    if (ofile) {
                        auto bytes = model->serialize(ofile);
                        ofile.close();
                        auto nodes1 = model->numNodes();
                        auto leaf1 = model->numLeafNodes();
                        Log::debug(
                            enUS_UTF8,
                            "Serialized {:Ld} bytes, {:Ld} nodes, {:Ld} leaf-nodes",
                            bytes, nodes1, leaf1);
                    } else {
                        Log::debug("failed to open file.");
                        messageBox("failed to open file.");
                    }
                } else {
                    Log::debug("No model loaded.");
                    messageBox("No model loaded.");
                }
            }
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
            renderer->initialize(graphicsContext, swapchain);
        }

        uiRenderer->registerTexture(volumeRenderer->texture);
        uiRenderer->registerTexture(volumeRenderer2->renderTarget);

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
            {
                auto t = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> d = t - timestamp;
                timestamp = t;
                delta = d.count();
            }

            for (auto& renderer : this->renderers) {
                renderer->update(delta);
            }

            do {
                if (this->isVisible.load() == false)
                    break;

                auto rp = swapchain->currentRenderPassDescriptor();

                auto& frontAttachment = rp.colorAttachments.front();
                frontAttachment.clearColor = Color::nonLinearGray;

                uint32_t width = frontAttachment.renderTarget->width();
                uint32_t height = frontAttachment.renderTarget->height();

                if (width > 0 && height > 0) {

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

                    auto view = ViewTransform(
                        camera.position, camera.target - camera.position,
                        Vector3(0, 1, 0));
                    auto projection = ProjectionTransform::perspective(
                        camera.fov,
                        float(width) / float(height),
                        camera.nearZ, camera.farZ);

                    for (auto& renderer : this->renderers) {
                        renderer->prepareScene(rp, view, projection);
                    }

                    ImGui::NewFrame();
                    this->uiLoop(delta);
                    ImGui::Render();

                    for (auto& renderer : this->renderers) {
                        renderer->render(rp, Rect(0, 0, width, height));
                    }
                }
                swapchain->FV::SwapChain::present();
            } while (0);

            auto t = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> d = t - timestamp;

            auto interval = std::max(frameInterval - d.count(), 0.0);
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
