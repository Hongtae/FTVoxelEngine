#include <limits>
#include <cmath>
#include "Scene.h"
#include "Mesh.h"
#include "Material.h"

using namespace FV;

void SceneNode::draw(RenderCommandEncoder* encoder, const SceneState& state) const
{
    if (mesh)
    {
        if (fabs(scale.x * scale.y * scale.z) > std::numeric_limits<float>::epsilon())
        {

        }
    }
}

Scene::Scene()
{

}

Scene::~Scene()
{

}
