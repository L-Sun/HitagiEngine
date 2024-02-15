#include <hitagi/asset/scene.hpp>
#include <hitagi/asset/transform.hpp>
#include <hitagi/math/transform.hpp>

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

}  // namespace hitagi::asset