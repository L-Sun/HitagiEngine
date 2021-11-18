#pragma once

#include "SceneNode.hpp"

namespace Hitagi::Asset {

class Scene {
private:
    std::shared_ptr<SceneObjectMaterial> m_DefaultMaterial;

public:
    std::shared_ptr<BaseSceneNode>                                      scene_graph;
    std::unordered_map<std::string, std::shared_ptr<SceneCameraNode>>   camera_nodes;
    std::unordered_map<std::string, std::shared_ptr<SceneLightNode>>    light_nodes;
    std::unordered_map<std::string, std::shared_ptr<SceneGeometryNode>> geometry_nodes;

    std::unordered_map<std::string, std::shared_ptr<SceneObjectCamera>>   cameras;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectLight>>    lights;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectMaterial>> materials;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectGeometry>> geometries;

public:
    Scene()
        : m_DefaultMaterial(std::make_shared<SceneObjectMaterial>("default")),
          scene_graph(std::make_shared<BaseSceneNode>("default")) {}
    Scene(std::string_view scene_name)
        : m_DefaultMaterial(std::make_shared<SceneObjectMaterial>("default")),
          scene_graph(std::make_shared<BaseSceneNode>(scene_name)) {}
    ~Scene() = default;

    std::vector<std::reference_wrapper<SceneGeometryNode>> GetGeometries() const;

    std::shared_ptr<SceneObjectCamera>   GetCamera(const std::string& key) const;
    std::shared_ptr<SceneObjectLight>    GetLight(const std::string& key) const;
    std::shared_ptr<SceneObjectGeometry> GetGeometry(const std::string& key) const;
    std::shared_ptr<SceneObjectMaterial> GetMaterial(const std::string& key) const;

    std::shared_ptr<SceneCameraNode> GetFirstCameraNode() const;
    std::shared_ptr<SceneLightNode>  GetFirstLightNode() const;

    void LoadResource();
};
}  // namespace Hitagi::Asset