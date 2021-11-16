#include "Scene.hpp"

namespace Hitagi::Asset {

std::vector<std::reference_wrapper<SceneGeometryNode>> Scene::GetGeometries() const {
    std::vector<std::reference_wrapper<SceneGeometryNode>> ret;
    for (auto&& [key, node] : geometry_nodes)
        ret.emplace_back(*node);
    return ret;
}

// Material
std::shared_ptr<SceneObjectMaterial> Scene::GetMaterial(const std::string& key) const {
    auto i = materials.find(key);
    if (i == materials.end())
        return nullptr;
    else
        return i->second;
}

// Geometry
std::shared_ptr<SceneObjectGeometry> Scene::GetGeometry(const std::string& key) const {
    auto i = geometries.find(key);
    if (i == geometries.end())
        return nullptr;
    else
        return i->second;
}

// Light
std::shared_ptr<SceneObjectLight> Scene::GetLight(const std::string& key) const {
    auto i = lights.find(key);
    if (i == lights.end())
        return nullptr;
    else
        return i->second;
}

// Camera
std::shared_ptr<SceneObjectCamera> Scene::GetCamera(const std::string& key) const {
    auto i = cameras.find(key);
    if (i == cameras.end())
        return nullptr;
    else
        return i->second;
}
std::shared_ptr<SceneCameraNode> Scene::GetFirstCameraNode() const {
    return (camera_nodes.empty() ? nullptr : camera_nodes.cbegin()->second);
}
std::shared_ptr<SceneLightNode> Scene::GetFirstLightNode() const {
    return (light_nodes.empty() ? nullptr : light_nodes.cbegin()->second);
}

void Scene::LoadResource() {
    for (auto& material : materials) {
        material.second->LoadTextures();
    }
}
}  // namespace Hitagi::Asset