export module physics:physics_manager;
import std;
import core;
import asset;
import ecs;
import math;
import :components;

namespace hitagi::physics {
struct ScenePhysicsSystem;
}

export namespace hitagi::physics {

struct PhysicsSettings {
    math::vec3f gravity{0.0f, 0.0f, -9.8f};

    float fixed_timestep = 1.0f / 60.0f;
    int   max_sub_steps  = 4;

    std::uint32_t max_bodies               = 65536;
    std::uint32_t max_body_pairs           = 65536;
    std::uint32_t max_contact_constraints  = 10240;
    std::size_t   temp_allocator_size      = 10 * 1024 * 1024;
};

class PhysicsManager final : public RuntimeModule {
public:
    explicit PhysicsManager(PhysicsSettings settings = {});
    ~PhysicsManager() final;

    static auto Get() -> PhysicsManager* { return static_cast<PhysicsManager*>(GetModule("PhysicsManager")); }

    void Tick() final;

    void Attach(asset::Scene& scene);
    void Detach(asset::Scene& scene);
    void OptimizeBroadPhase(asset::Scene& scene);

    void SetGravity(asset::Scene& scene, math::vec3f gravity);
    void AddForce(asset::Scene& scene, ecs::Entity entity, math::vec3f force);
    void AddImpulse(asset::Scene& scene, ecs::Entity entity, math::vec3f impulse);

    void SetDeltaTime(float delta_time) noexcept;

private:
    friend struct ScenePhysicsSystem;

    void EnsureBodyFromCollider(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body, const Collider& collider);
    void EnsureBodyFromMesh(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, const asset::MeshComponent& mesh_component, RigidBody& body);
    void SyncBodyFromTransform(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body);
    void StepScene(ecs::World& world);
    void PullBodyToTransform(ecs::World& world, asset::Transform& transform, const asset::RelationShip& relation_ship, RigidBody& body);
    void DestroyAttachedScene(ecs::World& world);

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

}  // namespace hitagi::physics
