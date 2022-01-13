#pragma once

#include "SceneNode.hpp"
#include "Animation.hpp"

#include <unordered_set>

namespace Hitagi::Asset {

class Scene {
private:
    std::string               m_Name;
    std::shared_ptr<Material> m_DefaultMaterial;

public:
    std::shared_ptr<SceneNode>                 scene_graph;
    std::vector<std::shared_ptr<CameraNode>>   camera_nodes;
    std::vector<std::shared_ptr<LightNode>>    light_nodes;
    std::vector<std::shared_ptr<GeometryNode>> geometry_nodes;
    std::vector<std::shared_ptr<BoneNode>>     bone_nodes;

    std::list<std::shared_ptr<Camera>>    cameras;
    std::list<std::shared_ptr<Light>>     lights;
    std::list<std::shared_ptr<Material>>  materials;
    std::list<std::shared_ptr<Geometry>>  geometries;
    std::list<std::shared_ptr<Animation>> animations;

public:
    Scene()
        : m_DefaultMaterial(std::make_shared<Material>()),
          scene_graph(std::make_shared<SceneNode>("root")) {}
    Scene(std::string_view scene_name)
        : m_Name(scene_name),
          m_DefaultMaterial(std::make_shared<Material>()),
          scene_graph(std::make_shared<SceneNode>("root")) {}
    ~Scene() = default;

    inline void               SetName(std::string name) noexcept { m_Name = std::move(name); }
    inline const std::string& GetName() const noexcept { return m_Name; }

    void AddSkeleton(std::shared_ptr<BoneNode> skeleton);
    void AddAnimation(std::shared_ptr<Animation> animation);

    inline const auto& GetGeometries() const noexcept { return geometry_nodes; }

    std::shared_ptr<CameraNode> GetFirstCameraNode() const;
    std::shared_ptr<LightNode>  GetFirstLightNode() const;

    void LoadResource();
};
}  // namespace Hitagi::Asset