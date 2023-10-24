#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <FVCore.h>

using namespace FV;

struct MaterialFace {
    struct Vertex {
        Vector3 pos;
        Vector2 uv;
        Vector4 color;
    };
    Vertex vertex[3];
    Material* material;
};

struct Model {
    struct Scene {
        std::string name;
        std::vector<SceneNode> nodes;
    };

    std::vector<Scene> scenes;
    int defaultSceneIndex;

    std::vector<Triangle> triangleList(int scene, GraphicsDeviceContext* graphicsContext) const;
    std::vector<MaterialFace> faceList(int scene, GraphicsDeviceContext* graphicsContext) const;
};

struct ForEachNode {
    SceneNode& node;
    void operator()(std::function<void(SceneNode&)> fn) {
        fn(node);
        for (auto& child : node.children)
            ForEachNode{ child }(fn);
    }
};

struct ForEachNodeConst {
    const SceneNode& node;
    void operator()(std::function<void(const SceneNode&)> fn) {
        fn(node);
        for (auto& child : node.children)
            ForEachNodeConst{ child }(fn);
    }
};

std::shared_ptr<Model> loadModel(std::filesystem::path path, FV::CommandQueue* queue);
std::optional<FV::MaterialShaderMap::Function> loadShader(std::filesystem::path path, FV::GraphicsDevice* device);
