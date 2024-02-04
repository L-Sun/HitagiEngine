#pragma once
#include <hitagi/asset/meta_info.hpp>
#include <hitagi/asset/material.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/light.hpp>
#include <hitagi/asset/mesh.hpp>
#include <hitagi/asset/skeleton.hpp>
#include <hitagi/ecs/world.hpp>

namespace hitagi::asset {
class Scene : public Resource {
public:
    Scene(std::string_view name = "");

    void Update();

    auto CreateEmptyEntity(math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateMeshEntity(std::shared_ptr<Mesh> mesh, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateCameraEntity(std::shared_ptr<Camera> camera, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateLightEntity(std::shared_ptr<Light> light, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateSkeletonEntity(std::shared_ptr<Skeleton> skeleton, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;

    auto& GetRootEntity() noexcept { return m_RootEntity; }
    auto& GetMeshEntities() noexcept { return m_MeshEntities; }
    auto& GetCameraEntities() noexcept { return m_CameraEntities; }
    auto& GetLightEntities() noexcept { return m_LightEntities; }

    auto GetCurrentCamera() const noexcept { return m_CurrentCamera; }

    auto GetWorld() noexcept -> ecs::World& { return m_World; }

private:
    ecs::World m_World;

    ecs::Entity m_CurrentCamera;

    ecs::Entity                   m_RootEntity;
    std::pmr::vector<ecs::Entity> m_MeshEntities;
    std::pmr::vector<ecs::Entity> m_CameraEntities;
    std::pmr::vector<ecs::Entity> m_LightEntities;
};

}  // namespace hitagi::asset