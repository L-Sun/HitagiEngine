#include "Scene.hpp"

namespace Hitagi::Asset {

std::vector<std::reference_wrapper<GeometryNode>> Scene::GetGeometries() const {
    std::vector<std::reference_wrapper<GeometryNode>> ret;
    for (auto&& [key, node] : geometry_nodes)
        ret.emplace_back(*node);
    return ret;
}

// Material
std::shared_ptr<Material> Scene::GetMaterial(const std::string& key) const {
    auto i = materials.find(key);
    if (i == materials.end())
        return nullptr;
    else
        return i->second;
}

// Geometry
std::shared_ptr<Geometry> Scene::GetGeometry(const std::string& key) const {
    auto i = geometries.find(key);
    if (i == geometries.end())
        return nullptr;
    else
        return i->second;
}

// Light
std::shared_ptr<Light> Scene::GetLight(const std::string& key) const {
    auto i = lights.find(key);
    if (i == lights.end())
        return nullptr;
    else
        return i->second;
}

// Camera
std::shared_ptr<Camera> Scene::GetCamera(const std::string& key) const {
    auto i = cameras.find(key);
    if (i == cameras.end())
        return nullptr;
    else
        return i->second;
}
std::shared_ptr<CameraNode> Scene::GetFirstCameraNode() const {
    return (camera_nodes.empty() ? nullptr : camera_nodes.cbegin()->second);
}
std::shared_ptr<LightNode> Scene::GetFirstLightNode() const {
    return (light_nodes.empty() ? nullptr : light_nodes.cbegin()->second);
}

void Scene::LoadResource() {
    for (auto& material : materials) {
        material.second->LoadTextures();
    }
}
}  // namespace Hitagi::Asset