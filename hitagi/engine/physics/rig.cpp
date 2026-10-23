export module physics:rig;
import interop.bullet;

import std;
import math;
import :types;
import :world;

export namespace hitagi::physics {

class RigWorld {
public:
    RigWorld(std::span<const RigBody> bodies, std::span<const RigJoint> joints, math::vec3f gravity,
             RigBackend backend = RigBackend::Bullet);
    ~RigWorld();
    RigWorld(const RigWorld&)                    = delete;
    auto operator=(const RigWorld&) -> RigWorld& = delete;
    void Reset(std::span<const math::mat4f> poses);
    void Step(std::span<const math::mat4f> anchors, float seconds);
    auto Pose(unsigned body) const -> math::mat4f;
    void SetPosition(unsigned body, math::vec3f position);
    auto MaxPenetration() const -> float;
    auto DeepestContact() const -> RigContact;

private:
    struct Impl;
    std::unique_ptr<Impl>         m_Impl;
    std::unique_ptr<JoltRigWorld> m_Jolt;
};
}  // namespace hitagi::physics

namespace hitagi::physics {
namespace bullet = interop::bullet;

auto RigBulletVector(math::vec3f p) -> bullet::btVector3 { return {p.x, p.y, p.z}; }
auto RigBulletTransform(const math::mat4f& m) -> bullet::btTransform {
    const auto [p, q, s] = math::decompose(m);
    return bullet::btTransform(bullet::btQuaternion(q.x, q.y, q.z, q.w), RigBulletVector(p));
}

struct RigWorld::Impl {
    bullet::btDefaultCollisionConfiguration                             configuration;
    bullet::btCollisionDispatcher                                       dispatcher{&configuration};
    bullet::btDbvtBroadphase                                            broadphase;
    bullet::btSequentialImpulseConstraintSolver                         solver;
    bullet::btDiscreteDynamicsWorld                                     world{&dispatcher, &broadphase, &solver, &configuration};
    std::vector<std::unique_ptr<bullet::btCollisionShape>>              shapes;
    std::vector<std::unique_ptr<bullet::btDefaultMotionState>>          states;
    std::vector<std::unique_ptr<bullet::btRigidBody>>                   bodies;
    std::vector<std::unique_ptr<bullet::btGeneric6DofSpringConstraint>> joints;
    std::vector<int>                                                    modes;
    ~Impl() {
        for (auto& joint : joints) world.removeConstraint(joint.get());
        for (auto& body : bodies) world.removeRigidBody(body.get());
    }
};

RigWorld::RigWorld(std::span<const RigBody> bodies, std::span<const RigJoint> joints, math::vec3f gravity,
                   RigBackend backend) {
    if (backend == RigBackend::Jolt) {
        m_Jolt = std::make_unique<JoltRigWorld>(bodies, joints, gravity);
        return;
    }
    m_Impl     = std::make_unique<Impl>();
    auto& data = *m_Impl;
    data.world.setGravity(RigBulletVector(gravity));
    data.world.getSolverInfo().m_numIterations = 20;
    for (const auto& desc : bodies) {
        if (desc.mode < 0 || desc.mode > 2 || desc.shape < 0 || desc.shape > 2 ||
            !std::isfinite(desc.mass) || desc.mass < 0.0f ||
            !std::isfinite(desc.size.norm()) || desc.size.x <= 0.0f ||
            (desc.shape != 0 && desc.size.y <= 0.0f) || (desc.shape == 1 && desc.size.z <= 0.0f))
            throw std::invalid_argument("Invalid rigid rig body");
        std::unique_ptr<bullet::btCollisionShape> shape;
        if (desc.shape == 0)
            shape = std::make_unique<bullet::btSphereShape>(desc.size.x);
        else if (desc.shape == 1)
            shape = std::make_unique<bullet::btBoxShape>(RigBulletVector(desc.size));
        else
            shape = std::make_unique<bullet::btCapsuleShape>(desc.size.x, desc.size.y);
        const float       mass = desc.mode == 0 ? 0.0f : desc.mass;
        bullet::btVector3 inertia(0, 0, 0);
        if (mass > 0) shape->calculateLocalInertia(mass, inertia);
        auto                                             state = std::make_unique<bullet::btDefaultMotionState>(RigBulletTransform(desc.bind));
        bullet::btRigidBody::btRigidBodyConstructionInfo info(mass, state.get(), shape.get(), inertia);
        info.m_friction    = desc.friction;
        info.m_restitution = desc.restitution;
        auto body          = std::make_unique<bullet::btRigidBody>(info);
        if (desc.mode == 0)
            body->setCollisionFlags(body->getCollisionFlags() | bullet::btCollisionObject::CF_KINEMATIC_OBJECT);
        body->setActivationState(bullet::disable_deactivation);
        body->setDamping(desc.linear_damping, desc.angular_damping);
        body->setSleepingThresholds(0, 0);
        body->setUserIndex(static_cast<int>(data.bodies.size()));
        data.world.addRigidBody(body.get(), static_cast<int>(desc.group), static_cast<int>(desc.mask));
        data.shapes.emplace_back(std::move(shape));
        data.states.emplace_back(std::move(state));
        data.bodies.emplace_back(std::move(body));
        data.modes.emplace_back(desc.mode);
    }
    for (const auto& desc : joints) {
        if (desc.a >= bodies.size() || desc.b >= bodies.size() || desc.a == desc.b)
            throw std::invalid_argument("Invalid rigid rig joint pair");
        const auto frame = RigBulletTransform(desc.bind);
        auto&      a     = *data.bodies[desc.a];
        auto&      b     = *data.bodies[desc.b];
        auto       joint = std::make_unique<bullet::btGeneric6DofSpringConstraint>(
            a, b, a.getWorldTransform().inverse() * frame, b.getWorldTransform().inverse() * frame, true);
        joint->setLinearLowerLimit(RigBulletVector(desc.linear_min));
        joint->setLinearUpperLimit(RigBulletVector(desc.linear_max));
        joint->setAngularLowerLimit(RigBulletVector(desc.angular_min));
        joint->setAngularUpperLimit(RigBulletVector(desc.angular_max));
        for (int axis = 0; axis < 6; ++axis) {
            const float stiffness = axis < 3 ? desc.linear_spring[axis] : desc.angular_spring[axis - 3];
            if (stiffness != 0.0f) {
                joint->enableSpring(axis, true);
                joint->setStiffness(axis, stiffness);
            }
            // Matches the established three.js MMDPhysics Bullet bridge.
            joint->setParam(bullet::stop_erp, 0.475f, axis);
        }
        data.world.addConstraint(joint.get(), true);
        data.joints.emplace_back(std::move(joint));
    }
}

RigWorld::~RigWorld() = default;

void RigWorld::Reset(std::span<const math::mat4f> poses) {
    if (m_Jolt) return m_Jolt->Reset(poses);
    auto& data = *m_Impl;
    if (poses.size() != data.bodies.size()) throw std::invalid_argument("Rig pose count mismatch");
    data.world.clearForces();
    for (unsigned i = 0; i < poses.size(); ++i) {
        auto&      body = *data.bodies[i];
        const auto pose = RigBulletTransform(poses[i]);
        body.setWorldTransform(pose);
        body.setInterpolationWorldTransform(pose);
        data.states[i]->setWorldTransform(pose);
        body.setLinearVelocity(bullet::btVector3(0, 0, 0));
        body.setAngularVelocity(bullet::btVector3(0, 0, 0));
        body.clearForces();
        body.activate(true);
        if (body.getBroadphaseHandle())
            data.broadphase.getOverlappingPairCache()->cleanProxyFromPairs(body.getBroadphaseHandle(), &data.dispatcher);
    }
    data.solver.reset();
    data.world.updateAabbs();
}

void RigWorld::Step(std::span<const math::mat4f> anchors, float seconds) {
    if (m_Jolt) return m_Jolt->Step(anchors, seconds);
    auto& data = *m_Impl;
    if (anchors.size() != data.bodies.size() || !std::isfinite(seconds) || seconds <= 0)
        throw std::invalid_argument("Invalid rigid rig step");
    for (unsigned i = 0; i < anchors.size(); ++i)
        if (data.modes[i] == 0) data.states[i]->setWorldTransform(RigBulletTransform(anchors[i]));
    data.world.stepSimulation(seconds, 0, seconds);
}

auto RigWorld::Pose(unsigned index) const -> math::mat4f {
    if (m_Jolt) return m_Jolt->Pose(index);
    const auto& pose = m_Impl->bodies.at(index)->getWorldTransform();
    const auto  p    = pose.getOrigin();
    const auto  q    = pose.getRotation();
    return math::translate(math::vec3f(p.x(), p.y(), p.z())) *
           math::rotate(math::quatf(q.x(), q.y(), q.z(), q.w()));
}

void RigWorld::SetPosition(unsigned index, math::vec3f position) {
    if (m_Jolt) return m_Jolt->SetPosition(index, position);
    auto& body = *m_Impl->bodies.at(index);
    auto  pose = body.getWorldTransform();
    pose.setOrigin(RigBulletVector(position));
    body.setWorldTransform(pose);
    m_Impl->states[index]->setWorldTransform(pose);
}

auto RigWorld::MaxPenetration() const -> float {
    return DeepestContact().penetration;
}
auto RigWorld::DeepestContact() const -> RigContact {
    if (m_Jolt) return m_Jolt->DeepestContact();
    RigContact contact;
    for (int i = 0; i < m_Impl->dispatcher.getNumManifolds(); ++i) {
        const auto* manifold = m_Impl->dispatcher.getManifoldByIndexInternal(i);
        // Overlaps between two animated bodies are authored pose geometry, not
        // contacts the dynamic solver can resolve.
        if (manifold->getBody0()->isStaticOrKinematicObject() &&
            manifold->getBody1()->isStaticOrKinematicObject()) continue;
        for (int j = 0; j < manifold->getNumContacts(); ++j) {
            // Manifold distance was measured before the solver/integration.
            // Evaluate its local witnesses at the current body poses instead.
            // This is a contact residual, not an exact mesh intersection query.
            const auto& point = manifold->getContactPoint(j);
            auto        delta = manifold->getBody0()->getWorldTransform()(point.m_localPointA);
            delta -= manifold->getBody1()->getWorldTransform()(point.m_localPointB);
            const float depth = -delta.dot(point.m_normalWorldOnB);
            if (depth > contact.penetration)
                contact = {depth, manifold->getBody0()->getUserIndex(), manifold->getBody1()->getUserIndex()};
        }
    }
    return contact;
}
}  // namespace hitagi::physics
