module;

#include <cassert>

module asset;
import std;

namespace hitagi::asset {

Scene::Scene(std::string_view name)
    : Resource(Type::Scene, name), m_World(name) {
    m_RootEntity = CreateEmptyEntity(math::mat4f::identity(), ecs::Entity(), name);
    m_World.GetSystemManager().Register<RelationShipSystem>();
    m_World.GetSystemManager().Register<TransformSystem>();
}

void Scene::Update() {
    m_World.Update();
}

auto Scene::CreateEmptyEntity(math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    auto& em = m_World.GetEntityManager();

    const auto [translation, rotation, scaling] = math::decompose(transform);
    if (!parent && m_RootEntity) parent = m_RootEntity;

    ecs::Entity entity = em.Create();
    entity.Emplace<MetaInfo>(name);
    entity.Emplace<Transform>(translation, rotation, scaling);
    entity.Emplace<RelationShip>(parent);

    return entity;
}

auto Scene::CreateMeshEntity(std::shared_ptr<Mesh> mesh, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(mesh != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<MeshComponent>();
    entity.Get<MeshComponent>().mesh = std::move(mesh);

    return m_MeshEntities.emplace_back(entity);
}

auto Scene::CreateCameraEntity(std::shared_ptr<Camera> camera, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(camera != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<CameraComponent>();
    entity.Get<CameraComponent>().camera = std::move(camera);

    m_CurrentCamera = entity;
    return m_CameraEntities.emplace_back(entity);
}

auto Scene::CreateLightEntity(std::shared_ptr<Light> light, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(light != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<LightComponent>();
    entity.Get<LightComponent>().light = std::move(light);

    return m_LightEntities.emplace_back(entity);
}

auto Scene::CreateSkeletonEntity(std::shared_ptr<Skeleton> skeleton, math::mat4f transform, ecs::Entity parent, std::string_view name) -> ecs::Entity {
    assert(skeleton != nullptr);

    auto entity = CreateEmptyEntity(transform, parent, name);
    entity.Emplace<SkeletonComponent>();
    entity.Get<SkeletonComponent>().skeleton = std::move(skeleton);

    return entity;
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
