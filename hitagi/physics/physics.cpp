module;

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdarg>
#include <cstdio>

module physics;

import std;

namespace hitagi::physics {

namespace {

std::mutex  g_JoltRuntimeMutex;
std::size_t g_JoltRuntimeRefCount = 0;

void JoltTrace(const char* fmt, ...) {
    char    buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    std::fprintf(stderr, "[Jolt] %s\n", buffer);
}

JPH_IF_ENABLE_ASSERTS(
    bool JoltAssertFailed(const char* expression, const char* message, const char* file, JPH::uint line) {
        std::fprintf(
            stderr,
            "[Jolt] Assertion failed: %s%s%s (%s:%u)\n",
            expression,
            message != nullptr ? " - " : "",
            message != nullptr ? message : "",
            file,
            line);
        return false;
    })

void AcquireJoltRuntime() {
    const std::scoped_lock lock(g_JoltRuntimeMutex);
    if (g_JoltRuntimeRefCount++ != 0) return;

    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTrace;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)
    if (JPH::Factory::sInstance == nullptr) {
        JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
}

void ReleaseJoltRuntime() {
    const std::scoped_lock lock(g_JoltRuntimeMutex);
    if (g_JoltRuntimeRefCount == 0 || --g_JoltRuntimeRefCount != 0) return;

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

namespace ObjectLayers {
constexpr JPH::ObjectLayer NonMoving = 0;
constexpr JPH::ObjectLayer Moving    = 1;
constexpr JPH::ObjectLayer Count     = 2;
}  // namespace ObjectLayers

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
constexpr JPH::uint            Count = 2;
}  // namespace BroadPhaseLayers

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface() {
        m_ObjectToBroadPhase[ObjectLayers::NonMoving] = BroadPhaseLayers::NonMoving;
        m_ObjectToBroadPhase[ObjectLayers::Moving]    = BroadPhaseLayers::Moving;
    }

    auto GetNumBroadPhaseLayers() const -> JPH::uint final {
        return BroadPhaseLayers::Count;
    }

    auto GetBroadPhaseLayer(JPH::ObjectLayer layer) const -> JPH::BroadPhaseLayer final {
        return m_ObjectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    auto GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const -> const char* final {
        switch (layer.GetValue()) {
            case 0:
                return "NonMoving";
            case 1:
                return "Moving";
            default:
                return "Unknown";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_ObjectToBroadPhase[ObjectLayers::Count];
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    auto ShouldCollide(JPH::ObjectLayer lhs, JPH::ObjectLayer rhs) const -> bool final {
        switch (lhs) {
            case ObjectLayers::NonMoving:
                return rhs == ObjectLayers::Moving;
            case ObjectLayers::Moving:
                return true;
            default:
                return false;
        }
    }
};

class ObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    auto ShouldCollide(JPH::ObjectLayer object_layer, JPH::BroadPhaseLayer broad_phase_layer) const -> bool final {
        switch (object_layer) {
            case ObjectLayers::NonMoving:
                return broad_phase_layer == BroadPhaseLayers::Moving;
            case ObjectLayers::Moving:
                return true;
            default:
                return false;
        }
    }
};

auto ToJolt(math::vec3f value) noexcept -> JPH::Vec3 {
    return JPH::Vec3(value.x, value.y, value.z);
}

auto ToJoltPosition(math::vec3f value) noexcept -> JPH::RVec3 {
    return JPH::RVec3(value.x, value.y, value.z);
}

auto ToJolt(math::quatf value) noexcept -> JPH::Quat {
    return JPH::Quat(value.x, value.y, value.z, value.w);
}

auto FromJolt(JPH::Vec3Arg value) noexcept -> math::vec3f {
    return math::vec3f(value.GetX(), value.GetY(), value.GetZ());
}

auto FromJoltPosition(JPH::RVec3Arg value) noexcept -> math::vec3f {
    return math::vec3f(
        static_cast<float>(value.GetX()),
        static_cast<float>(value.GetY()),
        static_cast<float>(value.GetZ()));
}

auto FromJolt(JPH::QuatArg value) noexcept -> math::quatf {
    return math::quatf(value.GetX(), value.GetY(), value.GetZ(), value.GetW());
}

auto ToJolt(MotionType motion_type) noexcept -> JPH::EMotionType {
    switch (motion_type) {
        case MotionType::Static:
            return JPH::EMotionType::Static;
        case MotionType::Kinematic:
            return JPH::EMotionType::Kinematic;
        case MotionType::Dynamic:
            return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Dynamic;
}

auto ToJolt(Activation activation) noexcept -> JPH::EActivation {
    return activation == Activation::Activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
}

auto ToObjectLayer(MotionType motion_type) noexcept -> JPH::ObjectLayer {
    return motion_type == MotionType::Static ? ObjectLayers::NonMoving : ObjectLayers::Moving;
}

auto ToJolt(BodyID body_id) noexcept -> JPH::BodyID {
    return body_id ? JPH::BodyID(body_id.value) : JPH::BodyID();
}

auto FromJolt(JPH::BodyID body_id) noexcept -> BodyID {
    return BodyID{body_id.GetIndexAndSequenceNumber()};
}

auto CreateShape(const ShapeDesc& desc) -> JPH::RefConst<JPH::Shape> {
    JPH::Shape::ShapeResult result;

    switch (desc.type) {
        case ShapeType::Box: {
            const JPH::BoxShapeSettings settings(ToJolt(desc.half_extent), desc.convex_radius);
            result = settings.Create();
            break;
        }
        case ShapeType::Sphere: {
            const JPH::SphereShapeSettings settings(desc.radius);
            result = settings.Create();
            break;
        }
    }

    if (result.HasError()) {
        throw std::runtime_error(result.GetError().c_str());
    }
    return result.Get();
}

}  // namespace

struct PhysicsWorld::Impl {
    explicit Impl(const PhysicsWorldDesc& desc)
        : temp_allocator(desc.temp_allocator_size),
          job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, desc.num_threads) {
        physics_system.Init(
            desc.max_bodies,
            desc.num_body_mutexes,
            desc.max_body_pairs,
            desc.max_contact_constraints,
            broad_phase_layer_interface,
            object_vs_broad_phase_layer_filter,
            object_layer_pair_filter);
        physics_system.SetGravity(ToJolt(desc.gravity));
    }

    BroadPhaseLayerInterface      broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilter object_vs_broad_phase_layer_filter;
    ObjectLayerPairFilter         object_layer_pair_filter;
    JPH::TempAllocatorImpl        temp_allocator;
    JPH::JobSystemThreadPool      job_system;
    JPH::PhysicsSystem            physics_system;
};

PhysicsWorld::PhysicsWorld(PhysicsWorldDesc desc) : core::RuntimeModule(desc.name) {
    AcquireJoltRuntime();
    m_Impl = std::make_unique<Impl>(desc);
}

PhysicsWorld::~PhysicsWorld() {
    m_Impl.reset();
    ReleaseJoltRuntime();
}

void PhysicsWorld::Tick() {
    core::RuntimeModule::Tick();
}

auto PhysicsWorld::Step(float delta_time, int collision_steps) -> PhysicsUpdateError {
    if (delta_time <= 0.0f) return PhysicsUpdateError::None;

    const auto error = m_Impl->physics_system.Update(
        delta_time,
        std::max(collision_steps, 1),
        &m_Impl->temp_allocator,
        &m_Impl->job_system);
    return static_cast<PhysicsUpdateError>(static_cast<std::uint32_t>(error));
}

void PhysicsWorld::OptimizeBroadPhase() {
    m_Impl->physics_system.OptimizeBroadPhase();
}

auto PhysicsWorld::CreateAndAddBody(const BodyDesc& desc, Activation activation) -> BodyID {
    const auto shape = CreateShape(desc.shape);

    JPH::BodyCreationSettings settings(
        shape.GetPtr(),
        ToJoltPosition(desc.transform.position),
        ToJolt(desc.transform.rotation),
        ToJolt(desc.motion_type),
        ToObjectLayer(desc.motion_type));

    settings.mLinearVelocity  = ToJolt(desc.linear_velocity);
    settings.mAngularVelocity = ToJolt(desc.angular_velocity);
    settings.mFriction        = desc.friction;
    settings.mRestitution     = desc.restitution;
    settings.mGravityFactor   = desc.gravity_factor;
    settings.mAllowSleeping   = desc.allow_sleeping;
    settings.mUserData        = desc.user_data;

    auto& body_interface = m_Impl->physics_system.GetBodyInterface();
    return FromJolt(body_interface.CreateAndAddBody(settings, ToJolt(activation)));
}

void PhysicsWorld::RemoveAndDestroyBody(BodyID body_id) {
    const auto jolt_body_id = ToJolt(body_id);
    if (jolt_body_id.IsInvalid()) return;

    auto& body_interface = m_Impl->physics_system.GetBodyInterface();
    if (body_interface.IsAdded(jolt_body_id)) {
        body_interface.RemoveBody(jolt_body_id);
    }
    body_interface.DestroyBody(jolt_body_id);
}

auto PhysicsWorld::IsBodyAdded(BodyID body_id) const -> bool {
    const auto jolt_body_id = ToJolt(body_id);
    return !jolt_body_id.IsInvalid() && m_Impl->physics_system.GetBodyInterface().IsAdded(jolt_body_id);
}

auto PhysicsWorld::GetNumBodies() const -> std::uint32_t {
    return static_cast<std::uint32_t>(m_Impl->physics_system.GetNumBodies());
}

void PhysicsWorld::SetBodyTransform(BodyID body_id, const BodyTransform& transform, Activation activation) {
    m_Impl->physics_system.GetBodyInterface().SetPositionAndRotationWhenChanged(
        ToJolt(body_id),
        ToJoltPosition(transform.position),
        ToJolt(transform.rotation),
        ToJolt(activation));
}

auto PhysicsWorld::GetBodyTransform(BodyID body_id) const -> BodyTransform {
    JPH::RVec3 position;
    JPH::Quat  rotation;
    m_Impl->physics_system.GetBodyInterface().GetPositionAndRotation(ToJolt(body_id), position, rotation);
    return BodyTransform{
        .position = FromJoltPosition(position),
        .rotation = FromJolt(rotation),
    };
}

void PhysicsWorld::SetLinearVelocity(BodyID body_id, math::vec3f velocity) {
    m_Impl->physics_system.GetBodyInterface().SetLinearVelocity(ToJolt(body_id), ToJolt(velocity));
}

auto PhysicsWorld::GetLinearVelocity(BodyID body_id) const -> math::vec3f {
    return FromJolt(m_Impl->physics_system.GetBodyInterface().GetLinearVelocity(ToJolt(body_id)));
}

void PhysicsWorld::SetAngularVelocity(BodyID body_id, math::vec3f velocity) {
    m_Impl->physics_system.GetBodyInterface().SetAngularVelocity(ToJolt(body_id), ToJolt(velocity));
}

auto PhysicsWorld::GetAngularVelocity(BodyID body_id) const -> math::vec3f {
    return FromJolt(m_Impl->physics_system.GetBodyInterface().GetAngularVelocity(ToJolt(body_id)));
}

void PhysicsWorld::AddForce(BodyID body_id, math::vec3f force, Activation activation) {
    m_Impl->physics_system.GetBodyInterface().AddForce(ToJolt(body_id), ToJolt(force), ToJolt(activation));
}

void PhysicsWorld::AddImpulse(BodyID body_id, math::vec3f impulse) {
    m_Impl->physics_system.GetBodyInterface().AddImpulse(ToJolt(body_id), ToJolt(impulse));
}

void PhysicsWorld::SetGravity(math::vec3f gravity) {
    m_Impl->physics_system.SetGravity(ToJolt(gravity));
}

auto PhysicsWorld::GetGravity() const -> math::vec3f {
    return FromJolt(m_Impl->physics_system.GetGravity());
}

}  // namespace hitagi::physics
