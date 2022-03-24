#pragma once
#include "IPhysicsManager.hpp"
#include "RigidBody.hpp"

#include <Math/Matrix.hpp>

namespace hitagi::physics {
class hitagiPhysicsManager : public IPhysicsManager {
public:
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    std::array<math::vec3f, 2> GetAABB(asset::GeometryNode& node) final;
    void                       CreateRigidBody(asset::GeometryNode& node) final;
    void                       DeleteRigidBody(asset::GeometryNode& node) final;

    math::mat4f GetRigidBodyTransform(asset::GeometryNode& node) final;
    void        UpdateRigidBodyTransform(asset::GeometryNode& node) final;

    void ApplyCentralForce(asset::GeometryNode& node, math::vec3f force) final;

private:
    std::unordered_map<std::string, RigidBody> m_RigidBodies;
};
}  // namespace hitagi::physics