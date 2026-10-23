module;
#include "interop/jolt_macros.hpp"

#include <cstdarg>
#include <cstdio>

export module physics:world;
import interop.jolt;

import std;
import core;
import math;

import :types;

export namespace hitagi::physics {

class PhysicsWorld final : public core::RuntimeModule {
public:
    // `job_system` drives Jolt's parallel simulation and must outlive the world.
    explicit PhysicsWorld(core::JobSystem& job_system, const PhysicsWorldDesc& desc = {});
    ~PhysicsWorld() final;

    PhysicsWorld(const PhysicsWorld&)            = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&)                 = delete;
    PhysicsWorld& operator=(PhysicsWorld&&)      = delete;

    void Tick() final;

    auto Step(float delta_time, int collision_steps = 1) -> PhysicsUpdateError;
    void OptimizeBroadPhase();

    auto CreateAndAddBody(const BodyDesc& desc, Activation activation = Activation::Activate) -> BodyID;
    void RemoveAndDestroyBody(BodyID body_id);

    auto IsBodyAdded(BodyID body_id) const -> bool;
    auto GetNumBodies() const -> std::uint32_t;

    void SetBodyTransform(BodyID body_id, const BodyTransform& transform, Activation activation = Activation::Activate);
    auto GetBodyTransform(BodyID body_id) const -> BodyTransform;

    void SetLinearVelocity(BodyID body_id, math::vec3f velocity);
    auto GetLinearVelocity(BodyID body_id) const -> math::vec3f;

    void SetAngularVelocity(BodyID body_id, math::vec3f velocity);
    auto GetAngularVelocity(BodyID body_id) const -> math::vec3f;

    void AddForce(BodyID body_id, math::vec3f force, Activation activation = Activation::Activate);
    void AddImpulse(BodyID body_id, math::vec3f impulse);

    void SetGravity(math::vec3f gravity);
    auto GetGravity() const -> math::vec3f;

private:
    friend class JoltRigWorld;
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

class JoltRigWorld {
public:
    JoltRigWorld(std::span<const RigBody>, std::span<const RigJoint>, math::vec3f gravity);
    ~JoltRigWorld();
    void Reset(std::span<const math::mat4f> poses);
    void Step(std::span<const math::mat4f> anchors, float seconds);
    auto Pose(unsigned body) const -> math::mat4f;
    void SetPosition(unsigned body, math::vec3f position);
    auto DeepestContact() const -> RigContact;
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

}  // namespace hitagi::physics

namespace hitagi::physics {


std::mutex  g_JoltRuntimeMutex;
std::size_t g_JoltRuntimeRefCount = 0;

void JoltTrace(const char* fmt, ...) {  // NOLINT(modernize-avoid-variadic-functions): signature required by JPH::Trace
    std::array<char, 1024> buffer{};
    va_list                args;
    va_start(args, fmt);
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);
    std::println(stderr, "[Jolt] {}", buffer.data());
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
    std::array<JPH::BroadPhaseLayer, ObjectLayers::Count> m_ObjectToBroadPhase;
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

class CoreJobSystemAdapter final : public JPH::JobSystemWithBarrier {
public:
    CoreJobSystemAdapter(core::JobSystem& job_system, JPH::uint max_barriers)
        : JPH::JobSystemWithBarrier(max_barriers),
          m_JobSystem(job_system) {}

    auto GetMaxConcurrency() const -> int final {
        return static_cast<int>(m_JobSystem.GetWorkerCount()) + 1;
    }

    auto CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0) -> JobHandle final {
        return JobHandle(new Job(inName, inColor, this, inJobFunction, inNumDependencies));
    }

protected:
    void QueueJob(Job* inJob) final {
        QueueJobs(&inJob, 1);
    }

    void QueueJobs(Job** inJobs, JPH::uint inNumJobs) final {
        for (JPH::uint index = 0; index < inNumJobs; ++index) {
            auto* job = inJobs[index];
            job->AddRef();
            try {
                m_JobSystem.Submit([job] {
                    job->Execute();
                    job->Release();
                });
            } catch (...) {
                job->Release();
                throw;
            }
        }
    }

    void FreeJob(Job* inJob) final {
        delete inJob;
    }

private:
    core::JobSystem& m_JobSystem;
};

auto ToJolt(math::vec3f value) noexcept -> JPH::Vec3 {
    return {value.x, value.y, value.z};
}

auto ToJoltPosition(math::vec3f value) noexcept -> JPH::RVec3 {
    return {value.x, value.y, value.z};
}

auto ToJolt(math::quatf value) noexcept -> JPH::Quat {
    return {value.x, value.y, value.z, value.w};
}

auto FromJolt(JPH::Vec3Arg value) noexcept -> math::vec3f {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

auto FromJoltPosition(JPH::RVec3Arg value) noexcept -> math::vec3f {
    return {
        static_cast<float>(value.GetX()),
        static_cast<float>(value.GetY()),
        static_cast<float>(value.GetZ())};
}

auto FromJolt(JPH::QuatArg value) noexcept -> math::quatf {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
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


struct PhysicsWorld::Impl {
    Impl(core::JobSystem& core_job_system, const PhysicsWorldDesc& desc)
        : temp_allocator(desc.temp_allocator_size),
          job_system(core_job_system, interop::max_physics_barriers) {
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
    CoreJobSystemAdapter          job_system;
    JPH::PhysicsSystem            physics_system;
};

PhysicsWorld::PhysicsWorld(core::JobSystem& job_system, const PhysicsWorldDesc& desc) : core::RuntimeModule(desc.name) {
    AcquireJoltRuntime();
    m_Impl = std::make_unique<Impl>(job_system, desc);
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


struct JoltRigWorld::Impl {
    // Reuse the engine's Jolt lifetime, broadphase and task adapter.
    core::JobSystem jobs{1};
    PhysicsWorld world;
    std::vector<RigBody> descriptions;
    std::vector<JPH::Body*> bodies;
    std::vector<JPH::Ref<JPH::SixDOFConstraint>> joints;
    JPH::Ref<JPH::GroupFilterTable> filter;
    struct Contacts final : JPH::ContactListener {
        struct Witness { unsigned a, b; JPH::Vec3 local_a, local_b, normal; };
        std::mutex mutex;
        std::vector<Witness> witnesses;
        void Capture(const JPH::Body& a, const JPH::Body& b,
                     const JPH::ContactManifold& m, JPH::ContactSettings& settings) {
            settings.mCombinedFriction = std::clamp(a.GetFriction() * b.GetFriction(), 0.0f, 10.0f);
            settings.mCombinedRestitution = a.GetRestitution() * b.GetRestitution();
            const std::scoped_lock lock(mutex);
            for (unsigned i = 0; i < m.mRelativeContactPointsOn1.size(); ++i)
                witnesses.emplace_back(unsigned(a.GetUserData()), unsigned(b.GetUserData()),
                                       JPH::Vec3(a.GetInverseCenterOfMassTransform() * m.GetWorldSpaceContactPointOn1(i)),
                                       JPH::Vec3(b.GetInverseCenterOfMassTransform() * m.GetWorldSpaceContactPointOn2(i)),
                                       m.mWorldSpaceNormal);
        }
        void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
                            const JPH::ContactManifold& m, JPH::ContactSettings& s) override { Capture(a,b,m,s); }
        void OnContactPersisted(const JPH::Body& a, const JPH::Body& b,
                                const JPH::ContactManifold& m, JPH::ContactSettings& s) override { Capture(a,b,m,s); }
    } contacts;
    explicit Impl(math::vec3f gravity) : world(jobs, PhysicsWorldDesc{
        .name="Character Jolt", .gravity=gravity, .max_body_pairs=4096, .max_contact_constraints=4096}) {}
    ~Impl() {
        auto& system = world.m_Impl->physics_system;
        system.SetContactListener(nullptr);
        for (auto& joint : joints) system.RemoveConstraint(joint.GetPtr());
        joints.clear();
        for (auto* body : bodies) {
            const auto id = body->GetID();
            system.GetBodyInterface().RemoveBody(id);
            system.GetBodyInterface().DestroyBody(id);
        }
    }
};

JoltRigWorld::JoltRigWorld(std::span<const RigBody> bodies, std::span<const RigJoint> joints, math::vec3f gravity)
    : m_Impl(std::make_unique<Impl>(gravity)) {
    auto& d = *m_Impl;
    auto& system = d.world.m_Impl->physics_system;
    auto settings = system.GetPhysicsSettings();
    settings.mNumVelocitySteps = 20;
    system.SetPhysicsSettings(settings);
    system.SetContactListener(&d.contacts);
    d.descriptions.assign(bodies.begin(), bodies.end());
    d.filter = new JPH::GroupFilterTable(static_cast<unsigned>(bodies.size()));
    for (unsigned a = 0; a < bodies.size(); ++a)
        for (unsigned b = a + 1; b < bodies.size(); ++b)
            if (!(bodies[a].group & bodies[b].mask) || !(bodies[b].group & bodies[a].mask))
                d.filter->DisableCollision(a,b);
    for (const auto& joint : joints) {
        if (joint.a >= bodies.size() || joint.b >= bodies.size() || joint.a == joint.b)
            throw std::invalid_argument("Invalid rigid rig joint pair");
        d.filter->DisableCollision(joint.a,joint.b);
    }
    for (unsigned i = 0; i < bodies.size(); ++i) {
        const auto& desc = bodies[i];
        if (desc.mode < 0 || desc.mode > 2 || desc.shape < 0 || desc.shape > 2 ||
            !std::isfinite(desc.mass) || desc.mass < 0 || !std::isfinite(desc.size.norm()) ||
            desc.size.x <= 0 || (desc.shape != 0 && desc.size.y <= 0) || (desc.shape == 1 && desc.size.z <= 0))
            throw std::invalid_argument("Invalid rigid rig body");
        JPH::RefConst<JPH::Shape> shape;
        if (desc.shape == 0) shape = JPH::SphereShapeSettings(desc.size.x).Create().Get();
        else if (desc.shape == 1) shape = JPH::BoxShapeSettings(ToJolt(desc.size),
            std::min({0.04f, desc.size.x, desc.size.y, desc.size.z})).Create().Get();
        else shape = JPH::CapsuleShapeSettings(desc.size.y * 0.5f, desc.size.x).Create().Get();
        const auto [p,q,s] = math::decompose(desc.bind);
        const bool dynamic = desc.mode != 0 && desc.mass > 0;
        JPH::BodyCreationSettings creation(shape.GetPtr(), ToJoltPosition(p), ToJolt(q),
            dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Kinematic,
            dynamic ? ObjectLayers::Moving : ObjectLayers::NonMoving);
        creation.mAllowSleeping = false;
        creation.mFriction = desc.friction;
        creation.mRestitution = desc.restitution;
        creation.mLinearDamping = creation.mAngularDamping = 0;
        creation.mUserData = i;
        creation.mCollisionGroup = JPH::CollisionGroup(d.filter.GetPtr(), 0, i);
        if (dynamic) {
            creation.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            creation.mMassPropertiesOverride.mMass = desc.mass;
        }
        auto* body = system.GetBodyInterface().CreateBody(creation);
        if (!body) throw std::runtime_error("Jolt rig body allocation failed");
        d.bodies.emplace_back(body);
        system.GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::Activate);
    }
    for (const auto& desc : joints) {
        JPH::SixDOFConstraintSettings c;
        c.mSpace = JPH::EConstraintSpace::WorldSpace;
        c.mSwingType = JPH::ESwingType::Pyramid;
        const auto [p,q,s] = math::decompose(desc.bind);
        c.mPosition1 = c.mPosition2 = ToJoltPosition(p);
        c.mAxisX1 = c.mAxisX2 = ToJolt(q) * JPH::Vec3::sAxisX();
        c.mAxisY1 = c.mAxisY2 = ToJolt(q) * JPH::Vec3::sAxisY();
        for (unsigned axis = 0; axis < 6; ++axis) {
            const auto a = static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis);
            // Bullet Generic6Dof's Euler extraction has the opposite sign to
            // Jolt's physical twist/swing angle. Preserve asymmetric intervals.
            const float lo = axis < 3 ? desc.linear_min[axis] : -desc.angular_max[axis-3];
            const float hi = axis < 3 ? desc.linear_max[axis] : -desc.angular_min[axis-3];
            if (lo > hi) c.MakeFreeAxis(a);
            else if (lo == hi) {
                if (lo != 0) throw std::invalid_argument("Nonzero locked Jolt axis requires a shifted frame");
                c.MakeFixedAxis(a);
            } else c.SetLimitedAxis(a, lo, hi);
            auto& spring = c.mMotorSettings[axis].mSpringSettings;
            spring.mMode = JPH::ESpringMode::StiffnessAndDamping;
            spring.mStiffness = axis < 3 ? desc.linear_spring[axis] : desc.angular_spring[axis-3];
            spring.mDamping = 0; // PMX has no SI joint viscous damping parameter.
        }
        JPH::Ref<JPH::SixDOFConstraint> joint = static_cast<JPH::SixDOFConstraint*>(
            c.Create(*d.bodies[desc.a], *d.bodies[desc.b]));
        for (unsigned axis = 0; axis < 6; ++axis)
            if (c.mMotorSettings[axis].mSpringSettings.mStiffness > 0 && c.mLimitMin[axis] < c.mLimitMax[axis])
                joint->SetMotorState(static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis), JPH::EMotorState::Position);
        system.AddConstraint(joint.GetPtr());
        d.joints.emplace_back(joint);
    }
    d.world.OptimizeBroadPhase();
}
JoltRigWorld::~JoltRigWorld() = default;
void JoltRigWorld::Reset(std::span<const math::mat4f> poses) {
    auto& d = *m_Impl;
    if (poses.size() != d.bodies.size()) throw std::invalid_argument("Rig pose count mismatch");
    auto& bi = d.world.m_Impl->physics_system.GetBodyInterface();
    for (unsigned i = 0; i < poses.size(); ++i) {
        const auto [p,q,s] = math::decompose(poses[i]);
        const auto id = d.bodies[i]->GetID();
        bi.SetPositionAndRotation(id,ToJoltPosition(p),ToJolt(q),JPH::EActivation::Activate);
        bi.SetLinearAndAngularVelocity(id,JPH::Vec3::sZero(),JPH::Vec3::sZero());
        bi.InvalidateContactCache(id);
    }
    for (auto& joint : d.joints) joint->ResetWarmStart();
    d.contacts.witnesses.clear();
}
void JoltRigWorld::Step(std::span<const math::mat4f> anchors, float seconds) {
    auto& d = *m_Impl;
    if (anchors.size() != d.bodies.size() || !std::isfinite(seconds) || seconds <= 0)
        throw std::invalid_argument("Invalid rigid rig step");
    auto& bi = d.world.m_Impl->physics_system.GetBodyInterface();
    for (unsigned i = 0; i < anchors.size(); ++i) {
        auto* body = d.bodies[i];
        if (d.descriptions[i].mode == 0) {
            const auto [p,q,s] = math::decompose(anchors[i]);
            bi.MoveKinematic(body->GetID(),ToJoltPosition(p),ToJolt(q),seconds);
        } else if (body->IsDynamic()) {
            auto coefficient = [seconds](float v) { return (1.0f-std::pow(1.0f-std::clamp(v,0.0f,1.0f),seconds))/seconds; };
            body->GetMotionProperties()->SetLinearDamping(coefficient(d.descriptions[i].linear_damping));
            body->GetMotionProperties()->SetAngularDamping(coefficient(d.descriptions[i].angular_damping));
        }
    }
    d.contacts.witnesses.clear();
    if (d.world.Step(seconds) != PhysicsUpdateError::None)
        throw std::runtime_error("Jolt rig exceeded simulation capacity");
}
auto JoltRigWorld::Pose(unsigned index) const -> math::mat4f {
    const auto* body = m_Impl->bodies.at(index);
    return math::translate(FromJoltPosition(body->GetPosition())) * math::rotate(FromJolt(body->GetRotation()));
}
void JoltRigWorld::SetPosition(unsigned index, math::vec3f position) {
    m_Impl->world.m_Impl->physics_system.GetBodyInterface().SetPosition(
        m_Impl->bodies.at(index)->GetID(),ToJoltPosition(position),JPH::EActivation::Activate);
}
auto JoltRigWorld::DeepestContact() const -> RigContact {
    RigContact result;
    for (const auto& w : m_Impl->contacts.witnesses) {
        const auto a = m_Impl->bodies[w.a]->GetCenterOfMassTransform() * w.local_a;
        const auto b = m_Impl->bodies[w.b]->GetCenterOfMassTransform() * w.local_b;
        const float depth = JPH::Vec3(a-b).Dot(w.normal);
        if (depth > result.penetration) result = {depth, int(w.a), int(w.b)};
    }
    return result;
}

}  // namespace hitagi::physics
