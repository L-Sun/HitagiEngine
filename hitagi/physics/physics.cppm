export module physics;
import std;
import math;
import ecs;
import core;
import asset;

export namespace hitagi::physics {

inline constexpr std::uint32_t invalid_body_id = std::numeric_limits<std::uint32_t>::max();

enum struct ObjectLayer : std::uint8_t {
    NonMoving = 0,
    Moving    = 1,
};

enum struct MotionType : std::uint8_t {
    Static    = 0,
    Kinematic = 1,
    Dynamic   = 2,
};

constexpr auto default_object_layer(MotionType motion_type) noexcept -> ObjectLayer {
    return motion_type == MotionType::Static ? ObjectLayer::NonMoving : ObjectLayer::Moving;
}

struct PhysicsMaterial {
    float friction    = 0.2f;
    float restitution = 0.0f;
};
static_assert(ecs::Component<PhysicsMaterial>);

struct BoxCollider {
    math::vec3f half_extent{0.5f};
    math::vec3f offset{0.0f};
};
static_assert(ecs::Component<BoxCollider>);

struct SphereCollider {
    float       radius = 0.5f;
    math::vec3f offset{0.0f};
};
static_assert(ecs::Component<SphereCollider>);

using ColliderShape = std::variant<BoxCollider, SphereCollider>;

struct Collider {
    ColliderShape shape = BoxCollider{};

    Collider() = default;
    Collider(BoxCollider box) : shape(std::move(box)) {}
    Collider(SphereCollider sphere) : shape(std::move(sphere)) {}
};
static_assert(ecs::Component<Collider>);

struct RigidBody {
    MotionType      motion_type  = MotionType::Static;
    ObjectLayer     object_layer = default_object_layer(motion_type);
    PhysicsMaterial material{};

    math::vec3f linear_velocity{0.0f};
    math::vec3f angular_velocity{0.0f};

    float gravity_factor = 1.0f;
    bool  activate       = true;
    bool  allow_sleep    = true;
    bool  is_sensor      = false;

    std::uint32_t body_id = invalid_body_id;

    math::vec3f cached_position{0.0f};
    math::quatf cached_rotation = math::quatf::identity();
    math::vec3f cached_scaling{1.0f};
    bool        initialized = false;

    RigidBody() = default;
    explicit RigidBody(MotionType motion_type)
        : motion_type(motion_type), object_layer(default_object_layer(motion_type)) {}
    RigidBody(MotionType motion_type, ObjectLayer object_layer) : motion_type(motion_type), object_layer(object_layer) {}

    auto Valid() const noexcept -> bool { return body_id != invalid_body_id; }
};
static_assert(ecs::Component<RigidBody>);


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
