export module asset:scene;
import std;
import ecs;
import core;
import :resource;
import :mesh;
import :transform;
import :animation;
import :skin;

export namespace hitagi::asset {

struct ScenePlugin {
    void Build(ecs::World& world) const {
        world.RegisterSystem<RelationShipSystem, AnimationSystem, TransformSystem, SkinningSystem>();
    }
};

class Scene : public Resource {
public:
    explicit Scene(std::string_view name = "");
    void Load(const ResourceLoadContext& context) final;
    void Unload() final;
    auto GetWorld() noexcept -> ecs::World& { return m_World; }
    auto GetWorld() const noexcept -> const ecs::World& { return m_World; }

private:
    ecs::World m_World;
};

}  // namespace hitagi::asset

namespace hitagi::asset {

Scene::Scene(std::string_view name) : Resource(Type::Scene, name), m_World(name) {
    core::PluginSet<ecs::World> plugins;
    plugins.Add(m_World, ScenePlugin{});
}

void Scene::Load(const ResourceLoadContext& context) {
    // Child resources are idempotent; dynamic skin snapshots may have changed
    // even while the Scene itself remains loaded.
    for (auto entity : m_World.GetEntityManager().GetEntities<MeshComponent>()) {
        const auto& mesh = entity.Get<MeshComponent>().mesh;
        if (mesh) mesh->Load(context);
        if (entity.Has<SkinnedMesh>()) {
            const auto& vertices = entity.Get<SkinnedMesh>().deformed_vertices;
            if (vertices) vertices->Load(context);
        }
    }
    SetLoadState(ResourceLoadState::Loaded);
}

void Scene::Unload() {
    // No load-state guard: the renderer loads meshes/materials directly without
    // going through Scene::Load, so the state may be Unloaded while GPU data exists.
    for (auto entity : m_World.GetEntityManager().GetEntities<MeshComponent>()) {
        if (entity.Has<SkinnedMesh>()) {
            const auto& vertices = entity.Get<SkinnedMesh>().deformed_vertices;
            if (vertices) vertices->Unload();
        }
        auto& mesh = entity.Get<MeshComponent>().mesh;
        if (!mesh) continue;
        mesh->Unload();
        for (const auto& sub_mesh : ResolveSubMeshes(*mesh, entity.Get<MeshComponent>().material_overrides)) {
            // Materials (and their pipelines) release GPU data here; textures are
            // deliberately left alone since they may be shared across scenes and
            // are reclaimed when the last owner drops them.
            if (sub_mesh.material) sub_mesh.material->Unload();
        }
    }
    SetLoadState(ResourceLoadState::Unloaded);
}

}  // namespace hitagi::asset
