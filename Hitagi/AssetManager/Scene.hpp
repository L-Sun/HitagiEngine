#pragma once

#include "SceneNode.hpp"

namespace Hitagi::Asset {

class Scene {
private:
    std::string               m_Name;
    std::shared_ptr<Material> m_DefaultMaterial;

public:
    std::shared_ptr<SceneNode>                                     scene_graph;
    std::unordered_map<std::string, std::shared_ptr<CameraNode>>   camera_nodes;
    std::unordered_map<std::string, std::shared_ptr<LightNode>>    light_nodes;
    std::unordered_map<std::string, std::shared_ptr<GeometryNode>> geometry_nodes;
    std::unordered_map<std::string, std::shared_ptr<BoneNode>>     bone_nodes;

    std::unordered_map<std::string, std::shared_ptr<Camera>>   cameras;
    std::unordered_map<std::string, std::shared_ptr<Light>>    lights;
    std::unordered_map<std::string, std::shared_ptr<Material>> materials;
    std::unordered_map<std::string, std::shared_ptr<Geometry>> geometries;

public:
    Scene()
        : m_DefaultMaterial(std::make_shared<Material>("default")),
          scene_graph(std::make_shared<SceneNode>("root")) {}
    Scene(std::string_view scene_name)
        : m_Name(scene_name),
          m_DefaultMaterial(std::make_shared<Material>("default")),
          scene_graph(std::make_shared<SceneNode>("root")) {}
    ~Scene() = default;

    inline void               SetName(std::string name) noexcept { m_Name = std::move(name); }
    inline const std::string& GetName() const noexcept { return m_Name; }

    void AddSkeleton(std::shared_ptr<BoneNode> skeleton);

    std::vector<std::reference_wrapper<GeometryNode>> GetGeometries() const;

    std::shared_ptr<Camera>   GetCamera(const std::string& key) const;
    std::shared_ptr<Light>    GetLight(const std::string& key) const;
    std::shared_ptr<Geometry> GetGeometry(const std::string& key) const;
    std::shared_ptr<Material> GetMaterial(const std::string& key) const;

    std::shared_ptr<CameraNode> GetFirstCameraNode() const;
    std::shared_ptr<LightNode>  GetFirstLightNode() const;

    void LoadResource();
};
}  // namespace Hitagi::Asset