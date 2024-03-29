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

    std::array<math::vec3f, 2> GetAABB(asset::Geometry& node) final;
    void                       CreateRigidBody(asset::Geometry& node) final;
    void                       DeleteRigidBody(asset::Geometry& node) final;

    math::mat4f GetRigidBodyTransform(asset::Geometry& node) final;
    void        UpdateRigidBodyTransform(asset::Geometry& node) final;

    void ApplyCentralForce(asset::Geometry& node, math::vec3f force) final;

private:
    std::pmr::unordered_map<std::pmr::string, RigidBody> m_RigidBodies;
};
}  // namespace hitagi::physics