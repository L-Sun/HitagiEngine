#pragma once
#include "rigid_body.hpp"

#include <hitagi/physics/physics_manager.hpp>
#include <hitagi/math/matrix.hpp>

namespace hitagi::physics {
class hitagiPhysicsManager : public IPhysicsManager {
public:
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    std::array<math::vec3f, 2> GetAABB(resource::Geometry& node) final;
    void                       CreateRigidBody(resource::Geometry& node) final;
    void                       DeleteRigidBody(resource::Geometry& node) final;

    math::mat4f GetRigidBodyTransform(resource::Geometry& node) final;
    void        UpdateRigidBodyTransform(resource::Geometry& node) final;

    void ApplyCentralForce(resource::Geometry& node, math::vec3f force) final;

private:
    std::unordered_map<std::string, RigidBody> m_RigidBodies;
};
}  // namespace hitagi::physics