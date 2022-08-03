#pragma once
#include <hitagi/resource/transform.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/renderable.hpp>
// #include <hitagi/resource/scene_node.hpp>
#include <hitagi/resource/armature.hpp>
#include <hitagi/ecs/world.hpp>

#include <crossguid/guid.hpp>

namespace hitagi::resource {
struct MetaInfo {
    std::pmr::string name;
    xg::Guid         guid = xg::newGuid();
};

struct Hierarchy {
    ecs::Entity parentID = ecs::Entity::InvalidEntity();
};

struct Scene {
    Scene(std::string_view name = "");
    std::pmr::vector<Renderable> GetRenderables() const;

    Camera&       GetCurrentCamera();
    const Camera& GetCurrentCamera() const;

    // void Attach(ecs::Entity entity, ecs::Entity target);

    // template <typename... Components>
    // std::shared_ptr<SceneNode> CreateNode(ecs::Entity parent, MetaInfo meta, Transform trans, Components&&... components);

    std::pmr::string name;
    ecs::World       world;

    ecs::Entity root;

    std::pmr::vector<ecs::Entity> geometries;
    std::pmr::vector<ecs::Entity> cameras;
    std::pmr::vector<ecs::Entity> lights;
    std::pmr::vector<ecs::Entity> armatures;

    std::size_t curr_camera_index = 0;
};

// template <typename... Components>
// std::shared_ptr<SceneNode> Scene::CreateNode(ecs::Entity parent, MetaInfo meta, Transform trans, Components&&... components) {
//     auto node       = std::make_shared<SceneNode>();
//     node->entity    = world.CreateEntity(std::move(meta), trans, node, components...);
//     node->transform = &world.AccessEntity<Transform>(node->entity)->get();
//     Attach(node->entity, parent);
//     return node;
// }

}  // namespace hitagi::resource