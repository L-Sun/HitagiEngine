module;

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>

module physics;
import std;
import utils;

namespace hitagi::physics {

namespace {
constexpr std::uint16_t kNonMovingLayer = 0;
constexpr std::uint16_t kMovingLayer    = 1;
constexpr std::uint8_t  kNumLayers      = 2;
constexpr float         kMinExtent      = 0.05f;
constexpr float         kSyncEpsilon    = 1e-4f;

struct PhysicsSceneTag {};
static_assert(ecs::Component<PhysicsSceneTag>);

auto to_jolt(const math::vec3f& value) noexcept -> JPH::Vec3 {
    return {value.x, value.y, value.z};
}

auto to_jolt_r(const math::vec3f& value) noexcept -> JPH::RVec3 {
    return {value.x, value.y, value.z};
}

auto to_jolt(const math::quatf& value) noexcept -> JPH::Quat {
    return {value.x, value.y, value.z, value.w};
}

auto from_jolt(const JPH::Vec3& value) noexcept -> math::vec3f {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

auto from_jolt_position(const JPH::RVec3& value) noexcept -> math::vec3f {
    return {
        static_cast<float>(value.GetX()),
        static_cast<float>(value.GetY()),
        static_cast<float>(value.GetZ()),
    };
}

auto from_jolt(const JPH::Quat& value) noexcept -> math::quatf {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

auto to_jolt(MotionType motion_type) noexcept -> JPH::EMotionType {
    switch (motion_type) {
        case MotionType::Static:
            return JPH::EMotionType::Static;
        case MotionType::Kinematic:
            return JPH::EMotionType::Kinematic;
        case MotionType::Dynamic:
            return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Static;
}

auto to_jolt(ObjectLayer object_layer) noexcept -> JPH::ObjectLayer {
    return object_layer == ObjectLayer::Moving ? kMovingLayer : kNonMovingLayer;
}

auto component_equal(const math::vec3f& lhs, const math::vec3f& rhs) noexcept -> bool {
    return math::max(math::absolute(lhs - rhs)) <= kSyncEpsilon;
}

auto component_equal(const math::quatf& lhs, const math::quatf& rhs) noexcept -> bool {
    return std::abs(math::dot(lhs, rhs)) >= (1.0f - kSyncEpsilon);
}

auto clamp_extents(math::vec3f extents) noexcept -> math::vec3f {
    return math::max(math::absolute(extents), kMinExtent);
}

auto scaled_offset(const math::vec3f& offset, const math::vec3f& scaling) noexcept -> math::vec3f {
    return offset * scaling;
}

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        m_ObjectToBroadPhase[kNonMovingLayer] = JPH::BroadPhaseLayer(0);
        m_ObjectToBroadPhase[kMovingLayer]    = JPH::BroadPhaseLayer(1);
    }

    auto GetNumBroadPhaseLayers() const -> JPH::uint override {
        return kNumLayers;
    }

    auto GetBroadPhaseLayer(JPH::ObjectLayer layer) const -> JPH::BroadPhaseLayer override {
        return m_ObjectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    auto GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const -> const char* override {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(layer)) {
            case 0:
                return "NON_MOVING";
            case 1:
                return "MOVING";
            default:
                return "INVALID";
        }
    }
#endif

private:
    std::array<JPH::BroadPhaseLayer, kNumLayers> m_ObjectToBroadPhase{};
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    auto ShouldCollide(JPH::ObjectLayer object_layer, JPH::BroadPhaseLayer broad_phase_layer) const -> bool override {
        switch (object_layer) {
            case kNonMovingLayer:
                return broad_phase_layer == JPH::BroadPhaseLayer(1);
            case kMovingLayer:
                return true;
            default:
                return false;
        }
    }
};

struct SceneContext {
    explicit SceneContext(const PhysicsSettings& settings)
        : object_layer_pair_filter(kNumLayers) {
        object_layer_pair_filter.EnableCollision(kNonMovingLayer, kMovingLayer);
        object_layer_pair_filter.EnableCollision(kMovingLayer, kMovingLayer);

        physics_system.Init(
            settings.max_bodies,
            0,
            settings.max_body_pairs,
            settings.max_contact_constraints,
            broad_phase_layer_interface,
            object_vs_broad_phase_layer_filter,
            object_layer_pair_filter);
        physics_system.SetGravity(to_jolt(settings.gravity));
    }

    BroadPhaseLayerInterfaceImpl       broad_phase_layer_interface{};
    ObjectVsBroadPhaseLayerFilterImpl  object_vs_broad_phase_layer_filter{};
    JPH::ObjectLayerPairFilterTable    object_layer_pair_filter;
    JPH::PhysicsSystem                 physics_system;
    std::unordered_map<std::uint32_t, ecs::Entity> body_entities;
    float accumulator = 0.0f;
    bool  optimize_broad_phase_pending = true;
};

}  // namespace

struct PhysicsManager::Impl {
    explicit Impl(PhysicsSettings settings)
        : settings(std::move(settings)),
          temp_allocator(std::make_unique<JPH::TempAllocatorImpl>(static_cast<int>(this->settings.temp_allocator_size))),
          job_system(std::make_unique<JPH::JobSystemThreadPool>(
              JPH::cMaxPhysicsJobs,
              JPH::cMaxPhysicsBarriers,
              std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1u))) {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        delta_time = settings.fixed_timestep;
    }

    ~Impl() {
        scenes.clear();
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    auto EnsureSceneContext(ecs::World& world) -> SceneContext& {
        auto& context = scenes[&world];
        if (!context) {
            context = std::make_unique<SceneContext>(settings);
        }
        return *context;
    }

    auto FindSceneContext(ecs::World& world) -> SceneContext* {
        if (auto iter = scenes.find(&world); iter != scenes.end()) {
            return iter->second.get();
        }
        return nullptr;
    }

    void DestroySceneContext(ecs::World& world) {
        auto iter = scenes.find(&world);
        if (iter == scenes.end()) return;

        auto& context       = *iter->second;
        auto& body_interface = context.physics_system.GetBodyInterface();
        for (const auto& [body_key, _] : context.body_entities) {
            const JPH::BodyID body_id(body_key);
            if (body_interface.IsAdded(body_id)) {
                body_interface.RemoveBody(body_id);
            }
            body_interface.DestroyBody(body_id);
        }

        scenes.erase(iter);
    }

    auto OwnsBody(SceneContext& context, const ecs::Entity& entity, const RigidBody& body) const noexcept -> bool {
        if (!body.Valid()) return false;
        if (auto iter = context.body_entities.find(body.body_id); iter != context.body_entities.end()) {
            return iter->second == entity;
        }
        return false;
    }

    void ResetBodyState(RigidBody& body) const noexcept {
        body.body_id     = invalid_body_id;
        body.initialized = false;
    }

    void DestroyTrackedBody(SceneContext& context, RigidBody& body) {
        if (!body.Valid()) {
            body.initialized = false;
            return;
        }

        if (!context.body_entities.contains(body.body_id)) {
            ResetBodyState(body);
            return;
        }

        const JPH::BodyID body_id(body.body_id);
        auto&             body_interface = context.physics_system.GetBodyInterface();
        if (body_interface.IsAdded(body_id)) {
            body_interface.RemoveBody(body_id);
        }
        body_interface.DestroyBody(body_id);
        context.body_entities.erase(body.body_id);
        ResetBodyState(body);
    }

    auto CreateShape(const Collider& collider, const math::vec3f& scaling) const -> JPH::RefConst<JPH::Shape> {
        return std::visit(
            [&](const auto& shape) -> JPH::RefConst<JPH::Shape> {
                using ShapeType = std::decay_t<decltype(shape)>;
                if constexpr (std::same_as<ShapeType, BoxCollider>) {
                    const auto half_extent = clamp_extents(shape.half_extent * math::absolute(scaling));
                    const auto offset      = scaled_offset(shape.offset, scaling);
                    const JPH::Shape* base_shape = new JPH::BoxShape(to_jolt(half_extent));
                    if (component_equal(offset, math::vec3f(0.0f))) {
                        return JPH::RefConst<JPH::Shape>(base_shape);
                    }
                    return JPH::RefConst<JPH::Shape>(new JPH::RotatedTranslatedShape(to_jolt(offset), JPH::Quat::sIdentity(), base_shape));
                } else {
                    const auto abs_scaling = math::absolute(scaling);
                    const auto radius      = std::max(shape.radius * math::max(abs_scaling), kMinExtent);
                    const auto offset      = scaled_offset(shape.offset, scaling);
                    const JPH::Shape* base_shape = new JPH::SphereShape(radius);
                    if (component_equal(offset, math::vec3f(0.0f))) {
                        return JPH::RefConst<JPH::Shape>(base_shape);
                    }
                    return JPH::RefConst<JPH::Shape>(new JPH::RotatedTranslatedShape(to_jolt(offset), JPH::Quat::sIdentity(), base_shape));
                }
            },
            collider.shape);
    }

    auto CreateShape(const asset::MeshComponent& mesh_component, const math::vec3f& scaling) const -> JPH::RefConst<JPH::Shape> {
        math::vec3f extents{0.5f};
        math::vec3f offset{0.0f};

        if (mesh_component.mesh && mesh_component.mesh->aabb.Valid()) {
            extents = mesh_component.mesh->aabb.Extents();
            offset  = mesh_component.mesh->aabb.Center();
        }

        const auto half_extent = clamp_extents(extents * math::absolute(scaling));
        const auto local_offset = scaled_offset(offset, scaling);
        const JPH::Shape* base_shape = new JPH::BoxShape(to_jolt(half_extent));
        if (component_equal(local_offset, math::vec3f(0.0f))) {
            return JPH::RefConst<JPH::Shape>(base_shape);
        }
        return JPH::RefConst<JPH::Shape>(new JPH::RotatedTranslatedShape(to_jolt(local_offset), JPH::Quat::sIdentity(), base_shape));
    }

    void CreateOrReplaceBody(SceneContext& context, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body, const JPH::Shape* shape) {
        if (OwnsBody(context, entity, body)) {
            DestroyTrackedBody(context, body);
        } else {
            ResetBodyState(body);
        }

        JPH::BodyCreationSettings body_settings(
            shape,
            to_jolt_r(transform.position),
            to_jolt(transform.rotation),
            to_jolt(body.motion_type),
            to_jolt(body.object_layer));

        body_settings.mUserData                    = entity.GetId();
        body_settings.mLinearVelocity              = to_jolt(body.linear_velocity);
        body_settings.mAngularVelocity             = to_jolt(body.angular_velocity);
        body_settings.mAllowSleeping               = body.allow_sleep;
        body_settings.mIsSensor                    = body.is_sensor;
        body_settings.mFriction                    = body.material.friction;
        body_settings.mRestitution                 = body.material.restitution;
        body_settings.mGravityFactor               = body.gravity_factor;
        body_settings.mAllowDynamicOrKinematic     = body.motion_type != MotionType::Dynamic;
        body_settings.mCollideKinematicVsNonDynamic = body.motion_type == MotionType::Kinematic || body.is_sensor;

        const auto activation = body.motion_type == MotionType::Static
                                    ? JPH::EActivation::DontActivate
                                    : (body.activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        auto& body_interface = context.physics_system.GetBodyInterface();
        auto  body_id        = body_interface.CreateAndAddBody(body_settings, activation);
        if (body_id.IsInvalid()) return;

        body.body_id         = body_id.GetIndexAndSequenceNumber();
        body.cached_position = transform.position;
        body.cached_rotation = transform.rotation;
        body.cached_scaling  = transform.scaling;
        body.initialized     = true;

        context.body_entities[body.body_id]        = entity;
        context.optimize_broad_phase_pending       = true;
    }

    void EnsureBodyFromCollider(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body, const Collider& collider) {
        auto& context = EnsureSceneContext(world);
        if (!OwnsBody(context, entity, body)) {
            const auto shape = CreateShape(collider, transform.scaling);
            CreateOrReplaceBody(context, entity, transform, body, shape.GetPtr());
        }
    }

    void EnsureBodyFromMesh(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, const asset::MeshComponent& mesh_component, RigidBody& body) {
        auto& context = EnsureSceneContext(world);
        if (!OwnsBody(context, entity, body)) {
            const auto shape = CreateShape(mesh_component, transform.scaling);
            CreateOrReplaceBody(context, entity, transform, body, shape.GetPtr());
        }
    }

    void SyncBodyFromTransform(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body) {
        auto* context = FindSceneContext(world);
        if (context == nullptr || !OwnsBody(*context, entity, body)) return;

        const bool scaling_changed =
            !component_equal(body.cached_scaling, transform.scaling);
        const bool position_changed =
            !component_equal(body.cached_position, transform.position);
        const bool rotation_changed =
            !component_equal(body.cached_rotation, transform.rotation);

        if (!scaling_changed && !position_changed && !rotation_changed) return;
        if (scaling_changed) {
            DestroyTrackedBody(*context, body);
            return;
        }

        auto& body_interface = context->physics_system.GetBodyInterface();
        const JPH::BodyID body_id(body.body_id);
        const auto        position = to_jolt_r(transform.position);
        const auto        rotation = to_jolt(transform.rotation);

        switch (body.motion_type) {
            case MotionType::Static:
                body_interface.SetPositionAndRotationWhenChanged(body_id, position, rotation, JPH::EActivation::DontActivate);
                break;
            case MotionType::Kinematic:
                body_interface.MoveKinematic(body_id, position, rotation, std::max(delta_time, settings.fixed_timestep));
                break;
            case MotionType::Dynamic:
                body_interface.SetPositionRotationAndVelocity(body_id, position, rotation, to_jolt(body.linear_velocity), to_jolt(body.angular_velocity));
                break;
        }

        body.cached_position = transform.position;
        body.cached_rotation = transform.rotation;
        body.cached_scaling  = transform.scaling;
        body.initialized     = true;
    }

    void CleanupDeadBodies(SceneContext& context) {
        auto& body_interface = context.physics_system.GetBodyInterface();
        std::pmr::vector<std::uint32_t> stale_bodies;
        for (const auto& [body_key, entity] : context.body_entities) {
            if (!entity.Valid() || !entity.Has<RigidBody>()) {
                stale_bodies.emplace_back(body_key);
                continue;
            }

            auto& rigid_body = entity.Get<RigidBody>();
            if (rigid_body.body_id != body_key) {
                stale_bodies.emplace_back(body_key);
            }
        }

        for (auto body_key : stale_bodies) {
            const JPH::BodyID body_id(body_key);
            if (auto iter = context.body_entities.find(body_key); iter != context.body_entities.end()) {
                if (iter->second.Valid() && iter->second.Has<RigidBody>()) {
                    auto& rigid_body = iter->second.Get<RigidBody>();
                    if (rigid_body.body_id == body_key) {
                        ResetBodyState(rigid_body);
                    }
                }
            }
            if (body_interface.IsAdded(body_id)) {
                body_interface.RemoveBody(body_id);
            }
            body_interface.DestroyBody(body_id);
            context.body_entities.erase(body_key);
        }
    }

    void StepWorld(ecs::World& world) {
        auto* context = FindSceneContext(world);
        if (context == nullptr) return;
        const auto logger = world.GetLogger();

        CleanupDeadBodies(*context);
        if (context->optimize_broad_phase_pending) {
            context->physics_system.OptimizeBroadPhase();
            context->optimize_broad_phase_pending = false;
        }

        const auto clamped_delta = std::clamp(delta_time, 0.0f, settings.fixed_timestep * settings.max_sub_steps);
        context->accumulator += clamped_delta > 0.0f ? clamped_delta : settings.fixed_timestep;

        int sub_steps = 0;
        while (context->accumulator + kSyncEpsilon >= settings.fixed_timestep && sub_steps < settings.max_sub_steps) {
            const auto error = context->physics_system.Update(settings.fixed_timestep, 1, temp_allocator.get(), job_system.get());
            if (error != JPH::EPhysicsUpdateError::None && logger != nullptr) {
                logger->warn("Jolt update returned error bits: {}", static_cast<std::uint32_t>(error));
            }
            context->accumulator -= settings.fixed_timestep;
            ++sub_steps;
        }

        if (sub_steps == settings.max_sub_steps && context->accumulator >= settings.fixed_timestep) {
            context->accumulator = std::fmod(context->accumulator, settings.fixed_timestep);
        }
    }

    void PullBodyToTransform(ecs::World& world, asset::Transform& transform, const asset::RelationShip& relation_ship, RigidBody& body) {
        auto* context = FindSceneContext(world);
        if (context == nullptr || !body.Valid() || !context->body_entities.contains(body.body_id)) return;
        if (body.motion_type == MotionType::Static) return;

        auto& body_interface = context->physics_system.GetBodyInterface();
        const JPH::BodyID body_id(body.body_id);

        JPH::RVec3 position;
        JPH::Quat  rotation;
        body_interface.GetPositionAndRotation(body_id, position, rotation);
        body.linear_velocity  = from_jolt(body_interface.GetLinearVelocity(body_id));
        body.angular_velocity = from_jolt(body_interface.GetAngularVelocity(body_id));

        const auto world_position = from_jolt_position(position);
        const auto world_rotation = from_jolt(rotation);
        const auto world_matrix   = math::translate(world_position) * math::rotate(world_rotation) * math::scale(transform.scaling);

        math::mat4f local_matrix = world_matrix;
        if (relation_ship.parent && relation_ship.parent.Has<asset::Transform>()) {
            local_matrix = math::inverse(relation_ship.parent.Get<asset::Transform>().world_matrix) * world_matrix;
        }

        // Apply the new pose immediately so rendering sees the simulated transform this frame.
        auto [local_position, local_rotation, local_scaling] = math::decompose(local_matrix);
        transform.position     = local_position;
        transform.rotation     = local_rotation;
        transform.scaling      = local_scaling;
        transform.local_matrix = local_matrix;
        transform.world_matrix = world_matrix;

        body.cached_position = local_position;
        body.cached_rotation = local_rotation;
        body.cached_scaling  = local_scaling;
        body.initialized     = true;
    }

    void SetGravity(asset::Scene& scene, math::vec3f gravity) {
        auto& context = EnsureSceneContext(scene.GetWorld());
        context.physics_system.SetGravity(to_jolt(gravity));
    }

    void AddForce(asset::Scene& scene, ecs::Entity entity, math::vec3f force) {
        auto* context = FindSceneContext(scene.GetWorld());
        if (context == nullptr || !entity.Valid() || !entity.Has<RigidBody>()) return;

        auto& body = entity.Get<RigidBody>();
        if (!OwnsBody(*context, entity, body)) return;

        context->physics_system.GetBodyInterface().AddForce(JPH::BodyID(body.body_id), to_jolt(force));
    }

    void AddImpulse(asset::Scene& scene, ecs::Entity entity, math::vec3f impulse) {
        auto* context = FindSceneContext(scene.GetWorld());
        if (context == nullptr || !entity.Valid() || !entity.Has<RigidBody>()) return;

        auto& body = entity.Get<RigidBody>();
        if (!OwnsBody(*context, entity, body)) return;

        context->physics_system.GetBodyInterface().AddImpulse(JPH::BodyID(body.body_id), to_jolt(impulse));
    }

    PhysicsSettings settings;
    float           delta_time = 1.0f / 60.0f;

    std::unique_ptr<JPH::TempAllocatorImpl>  temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system;
    std::unordered_map<ecs::World*, std::unique_ptr<SceneContext>> scenes;
};

struct ScenePhysicsSystem {
    static void OnCreate(ecs::World& world) {
        if (auto* manager = PhysicsManager::Get(); manager != nullptr) {
            manager->m_Impl->EnsureSceneContext(world);
        }

        auto entity = world.GetEntityManager().Create();
        entity.Emplace<PhysicsSceneTag>();
    }

    static void OnDestroy(ecs::World& world) {
        if (auto* manager = PhysicsManager::Get(); manager != nullptr) {
            manager->m_Impl->DestroySceneContext(world);
        }
    }

    static void OnUpdate(ecs::Schedule& schedule) {
        auto* manager = PhysicsManager::Get();
        if (manager == nullptr) return;

        schedule.SetOrder("update_world_matrix_flat", "physics_create_bodies_from_colliders");
        schedule.SetOrder("update_world_matrix_flat", "physics_create_bodies_from_meshes");
        schedule.SetOrder("physics_create_bodies_from_colliders", "physics_sync_bodies");
        schedule.SetOrder("physics_create_bodies_from_meshes", "physics_sync_bodies");
        schedule.SetOrder("physics_sync_bodies", "physics_step_world");
        schedule.SetOrder("physics_step_world", "physics_pull_bodies");

        schedule.Request(
            "physics_create_bodies_from_colliders",
            [manager, &schedule](const ecs::Entity entity, const asset::Transform& transform, RigidBody& body, const Collider& collider) {
                manager->EnsureBodyFromCollider(schedule.world, entity, transform, body, collider);
            });

        schedule.Request(
            "physics_create_bodies_from_meshes",
            [manager, &schedule](const ecs::Entity entity, const asset::Transform& transform, const asset::MeshComponent& mesh_component, RigidBody& body) {
                manager->EnsureBodyFromMesh(schedule.world, entity, transform, mesh_component, body);
            },
            {},
            ecs::filter::None<Collider>());

        schedule.Request(
            "physics_sync_bodies",
            [manager, &schedule](const ecs::Entity entity, const asset::Transform& transform, RigidBody& body) {
                manager->SyncBodyFromTransform(schedule.world, entity, transform, body);
            });

        schedule.Request(
            "physics_step_world",
            [manager, &schedule](PhysicsSceneTag&) {
                manager->StepScene(schedule.world);
            });

        schedule.Request(
            "physics_pull_bodies",
            [manager, &schedule](asset::Transform& transform, const asset::RelationShip& relation_ship, RigidBody& body) {
                manager->PullBodyToTransform(schedule.world, transform, relation_ship, body);
            });
    }
};

PhysicsManager::PhysicsManager(PhysicsSettings settings)
    : RuntimeModule("PhysicsManager"),
      m_Impl(std::make_unique<Impl>(std::move(settings))) {}

PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::Tick() {}

void PhysicsManager::EnsureBodyFromCollider(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body, const Collider& collider) {
    m_Impl->EnsureBodyFromCollider(world, entity, transform, body, collider);
}

void PhysicsManager::EnsureBodyFromMesh(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, const asset::MeshComponent& mesh_component, RigidBody& body) {
    m_Impl->EnsureBodyFromMesh(world, entity, transform, mesh_component, body);
}

void PhysicsManager::SyncBodyFromTransform(ecs::World& world, const ecs::Entity& entity, const asset::Transform& transform, RigidBody& body) {
    m_Impl->SyncBodyFromTransform(world, entity, transform, body);
}

void PhysicsManager::StepScene(ecs::World& world) {
    m_Impl->StepWorld(world);
}

void PhysicsManager::PullBodyToTransform(ecs::World& world, asset::Transform& transform, const asset::RelationShip& relation_ship, RigidBody& body) {
    m_Impl->PullBodyToTransform(world, transform, relation_ship, body);
}

void PhysicsManager::DestroyAttachedScene(ecs::World& world) {
    m_Impl->DestroySceneContext(world);
}

void PhysicsManager::Attach(asset::Scene& scene) {
    m_Impl->EnsureSceneContext(scene.GetWorld());
    scene.GetWorld().GetSystemManager().Register<ScenePhysicsSystem>();
}

void PhysicsManager::Detach(asset::Scene& scene) {
    scene.GetWorld().GetSystemManager().Unregister<ScenePhysicsSystem>();
}

void PhysicsManager::OptimizeBroadPhase(asset::Scene& scene) {
    auto& context = m_Impl->EnsureSceneContext(scene.GetWorld());
    context.optimize_broad_phase_pending = true;
}

void PhysicsManager::SetGravity(asset::Scene& scene, math::vec3f gravity) {
    m_Impl->SetGravity(scene, gravity);
}

void PhysicsManager::AddForce(asset::Scene& scene, ecs::Entity entity, math::vec3f force) {
    m_Impl->AddForce(scene, entity, force);
}

void PhysicsManager::AddImpulse(asset::Scene& scene, ecs::Entity entity, math::vec3f impulse) {
    m_Impl->AddImpulse(scene, entity, impulse);
}

void PhysicsManager::SetDeltaTime(float delta_time) noexcept {
    m_Impl->delta_time = delta_time;
}

}  // namespace hitagi::physics
