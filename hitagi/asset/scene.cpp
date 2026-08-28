module;

#include <cassert>

export module asset:scene;
import std;
import ecs;
import math;
import gfx;
import :resource;
import :mesh;
import :camera;
import :light;
import :transform;

export namespace hitagi::asset {

class Scene : public Resource {
public:
    explicit Scene(std::string_view name = "");

    void Update();
    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    auto CreateEmptyEntity(math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateMeshEntity(std::shared_ptr<Mesh> mesh, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateCameraEntity(std::shared_ptr<Camera> camera, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;
    auto CreateLightEntity(std::shared_ptr<Light> light, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity;

    void DestroyEntitySubtree(ecs::Entity entity);
    void ReparentEntity(ecs::Entity entity, ecs::Entity parent);
    void RenameEntity(ecs::Entity entity, std::string_view name);

    auto AddCameraComponent(ecs::Entity entity, std::shared_ptr<Camera> camera) -> bool;
    auto RemoveCameraComponent(ecs::Entity entity) -> std::shared_ptr<Camera>;
    auto AddLightComponent(ecs::Entity entity, std::shared_ptr<Light> light) -> bool;
    auto RemoveLightComponent(ecs::Entity entity) -> std::shared_ptr<Light>;

    auto& GetRootEntity() noexcept { return m_RootEntity; }
    auto  GetRootEntity() const noexcept { return m_RootEntity; }
    auto& GetMeshEntities() noexcept { return m_MeshEntities; }
    auto& GetCameraEntities() noexcept { return m_CameraEntities; }
    auto& GetLightEntities() noexcept { return m_LightEntities; }
    auto  GetCurrentCamera() const noexcept { return m_CurrentCamera; }
    auto  GetWorld() noexcept -> ecs::World& { return m_World; }
    auto  GetWorld() const noexcept -> const ecs::World& { return m_World; }

private:
    ecs::World                    m_World;
    ecs::Entity                   m_RootEntity;
    ecs::Entity                   m_CurrentCamera;
    std::pmr::vector<ecs::Entity> m_MeshEntities;
    std::pmr::vector<ecs::Entity> m_CameraEntities;
    std::pmr::vector<ecs::Entity> m_LightEntities;
};

}  // namespace hitagi::asset

namespace hitagi::asset {

Scene::Scene(std::string_view name)
    : Resource(Type::Scene, name),
      m_World(name) {
    m_RootEntity = CreateEmptyEntity(math::mat4f::identity(), ecs::Entity(), name);
    m_World.GetSystemManager().Register<RelationShipSystem>();
    m_World.GetSystemManager().Register<TransformSystem>();
}

void Scene::Update() {
    m_World.Update();
}

void Scene::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    for (auto entity : m_MeshEntities) {
        if (entity.Has<MeshComponent>()) {
            auto& mesh = entity.Get<MeshComponent>().mesh;
            if (mesh) mesh->Load(context);
        }
    }
    SetLoadState(ResourceLoadState::Loaded);
}

void Scene::Unload() {
    // No load-state guard: the renderer loads meshes/materials directly without
    // going through Scene::Load, so the state may be Unloaded while GPU data exists.
    for (auto entity : m_MeshEntities) {
        if (!entity.Has<MeshComponent>()) continue;
        auto& mesh = entity.Get<MeshComponent>().mesh;
        if (!mesh) continue;
        mesh->Unload();
        for (const auto& sub_mesh : mesh->sub_meshes) {
            // Materials (and their pipelines) release GPU data here; textures are
            // deliberately left alone since they may be shared across scenes and
            // are reclaimed when the last owner drops them.
            if (sub_mesh.material) sub_mesh.material->Unload();
        }
    }
    SetLoadState(ResourceLoadState::Unloaded);
}

auto Scene::CreateEmptyEntity(math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    auto& em = m_World.GetEntityManager();

    const auto [translation, rotation, scaling] = math::decompose(transform);
    if (!parent && m_RootEntity) parent = m_RootEntity;

    auto entity = em.Create();
    entity.Emplace<MetaInfo>(name);
    entity.Emplace<Transform>(translation, rotation, scaling);
    entity.Emplace<RelationShip>(parent);

    return entity;
}

auto Scene::CreateMeshEntity(std::shared_ptr<Mesh> mesh, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(mesh != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<MeshComponent>().mesh = std::move(mesh);

    return m_MeshEntities.emplace_back(entity);
}

auto Scene::CreateCameraEntity(std::shared_ptr<Camera> camera, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(camera != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<CameraComponent>().camera = std::move(camera);

    m_CurrentCamera = entity;
    return m_CameraEntities.emplace_back(entity);
}

auto Scene::CreateLightEntity(std::shared_ptr<Light> light, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(light != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<LightComponent>().light = std::move(light);

    return m_LightEntities.emplace_back(entity);
}

void Scene::DestroyEntitySubtree(ecs::Entity entity) {
    if (!entity || entity == m_RootEntity) return;

    if (entity.Has<RelationShip>()) {
        auto children = entity.Get<RelationShip>().GetChildren() | std::ranges::to<std::pmr::vector<ecs::Entity>>();
        for (auto child : children) {
            DestroyEntitySubtree(child);
        }

        auto& relation = entity.Get<RelationShip>();
        if (relation.parent && relation.parent.Has<RelationShip>()) {
            relation.parent.Get<RelationShip>().children.erase(entity);
        }
        relation.parent = {};
        relation.prev_parent = {};
    }

    std::erase(m_MeshEntities, entity);
    std::erase(m_CameraEntities, entity);
    std::erase(m_LightEntities, entity);
    if (m_CurrentCamera == entity) {
        m_CurrentCamera = m_CameraEntities.empty() ? ecs::Entity{} : m_CameraEntities.front();
    }

    m_World.GetEntityManager().Destroy(entity);
}

void Scene::ReparentEntity(ecs::Entity entity, ecs::Entity parent) {
    if (!entity || entity == m_RootEntity) return;
    if (!parent) parent = m_RootEntity;
    for (auto ancestor = parent; ancestor; ancestor = ancestor.Has<RelationShip>() ? ancestor.Get<RelationShip>().parent : ecs::Entity{}) {
        if (ancestor == entity) return;
    }
    if (!entity.Has<RelationShip>()) {
        entity.Emplace<RelationShip>(parent);
    } else {
        entity.Get<RelationShip>().parent = parent;
    }
    Update();
}

void Scene::RenameEntity(ecs::Entity entity, std::string_view name) {
    if (!entity) return;
    if (!entity.Has<MetaInfo>()) {
        entity.Emplace<MetaInfo>(name);
    } else {
        entity.Get<MetaInfo>().name = name;
    }
}

auto Scene::AddCameraComponent(ecs::Entity entity, std::shared_ptr<Camera> camera) -> bool {
    if (!entity || entity.Has<CameraComponent>()) return false;

    entity.Emplace<CameraComponent>().camera = std::move(camera);
    if (std::ranges::find(m_CameraEntities, entity) == m_CameraEntities.end()) {
        m_CameraEntities.emplace_back(entity);
    }
    if (!m_CurrentCamera) m_CurrentCamera = entity;
    return true;
}

auto Scene::RemoveCameraComponent(ecs::Entity entity) -> std::shared_ptr<Camera> {
    if (!entity || !entity.Has<CameraComponent>()) return {};

    auto camera = entity.Get<CameraComponent>().camera;
    entity.Remove<CameraComponent>();
    std::erase(m_CameraEntities, entity);
    if (m_CurrentCamera == entity) {
        m_CurrentCamera = m_CameraEntities.empty() ? ecs::Entity{} : m_CameraEntities.front();
    }
    return camera;
}

auto Scene::AddLightComponent(ecs::Entity entity, std::shared_ptr<Light> light) -> bool {
    if (!entity || entity.Has<LightComponent>()) return false;

    entity.Emplace<LightComponent>().light = std::move(light);
    if (std::ranges::find(m_LightEntities, entity) == m_LightEntities.end()) {
        m_LightEntities.emplace_back(entity);
    }
    return true;
}

auto Scene::RemoveLightComponent(ecs::Entity entity) -> std::shared_ptr<Light> {
    if (!entity || !entity.Has<LightComponent>()) return {};

    auto light = entity.Get<LightComponent>().light;
    entity.Remove<LightComponent>();
    std::erase(m_LightEntities, entity);
    return light;
}

}  // namespace hitagi::asset
