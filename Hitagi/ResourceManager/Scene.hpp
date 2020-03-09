#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "SceneObject.hpp"
#include "SceneNode.hpp"

namespace Hitagi::Resource {

class Scene {
private:
    std::shared_ptr<SceneObjectMaterial> m_DefaultMaterial;

public:
    std::shared_ptr<BaseSceneNode>                                      SceneGraph;
    std::unordered_map<std::string, std::shared_ptr<SceneCameraNode>>   CameraNodes;
    std::unordered_map<std::string, std::shared_ptr<SceneLightNode>>    LightNodes;
    std::unordered_map<std::string, std::shared_ptr<SceneGeometryNode>> GeometryNodes;

    std::unordered_map<std::string, std::shared_ptr<SceneObjectCamera>>   Cameras;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectLight>>    Lights;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectMaterial>> Materials;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectMesh>>     Meshes;
    std::unordered_map<std::string, std::shared_ptr<SceneObjectGeometry>> Geometries;

public:
    Scene() { m_DefaultMaterial = std::make_shared<SceneObjectMaterial>("default"); }
    Scene(std::string_view scene_name) : SceneGraph(std::make_shared<BaseSceneNode>(scene_name)) {}
    ~Scene(){};

    std::shared_ptr<SceneObjectCamera>   GetCamera(const std::string& key) const;
    std::shared_ptr<SceneObjectLight>    GetLight(const std::string& key) const;
    std::shared_ptr<SceneObjectGeometry> GetGeometry(const std::string& key) const;
    std::shared_ptr<SceneObjectMaterial> GetMaterial(const std::string& key) const;

    std::shared_ptr<SceneCameraNode> GetFirstCameraNode() const;
    std::shared_ptr<SceneLightNode>  GetFirstLightNode() const;

    void LoadResource();
};
}  // namespace Hitagi::Resource