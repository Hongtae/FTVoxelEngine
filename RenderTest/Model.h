#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <FVCore.h>


struct Model
{
    struct Scene
    {
        std::string name;
        std::vector<FV::SceneNode> nodes;
    };

    std::vector<Scene> scenes;
    int defaultSceneIndex;
};

std::shared_ptr<Model> loadModel(std::filesystem::path path, FV::MaterialShaderMap shader, FV::CommandQueue* queue);
std::optional<FV::MaterialShaderMap::Function> loadShader(std::filesystem::path path, FV::GraphicsDevice* device);
