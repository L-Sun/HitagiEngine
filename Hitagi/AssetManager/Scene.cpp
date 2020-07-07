#include "Scene.hpp"

namespace Hitagi::Asset {

std::vector<std::reference_wrapper<SceneGeometryNode>> Scene::GetGeometries() const {
    std::vector<std::reference_wrapper<SceneGeometryNode>> ret;
    for (auto&& [key, node] : GeometryNodes)
        ret.emplace_back(*node);
    return ret;
}

// Material
std::shared_ptr<SceneObjectMaterial> Scene::GetMaterial(const std::string& key) const {
    auto i = Materials.find(key);
    if (i == Materials.end())
        return nullptr;
    else
        return i->second;
}

// Geometry
std::shared_ptr<SceneObjectGeometry> Scene::GetGeometry(const std::string& key) const {
    auto i = Geometries.find(key);
    if (i == Geometries.end())
        return nullptr;
    else
        return i->second;
}

// Light
std::shared_ptr<SceneObjectLight> Scene::GetLight(const std::string& key) const {
    auto i = Lights.find(key);
    if (i == Lights.end())
        return nullptr;
    else
        return i->second;
}

// Camera
std::shared_ptr<SceneObjectCamera> Scene::GetCamera(const std::string& key) const {
    auto i = Cameras.find(key);
    if (i == Cameras.end())
        return nullptr;
    else
        return i->second;
}
std::shared_ptr<SceneCameraNode> Scene::GetFirstCameraNode() const {
    return (CameraNodes.empty() ? nullptr : CameraNodes.cbegin()->second);
}
std::shared_ptr<SceneLightNode> Scene::GetFirstLightNode() const {
    return (LightNodes.empty() ? nullptr : LightNodes.cbegin()->second);
}

void Scene::LoadResource() {
    for (auto& material : Materials) {
        material.second->LoadTextures();
    }
}
}  // namespace Hitagi::Asset