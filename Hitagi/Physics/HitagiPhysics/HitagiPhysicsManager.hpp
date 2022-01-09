#pragma once
#include "IPhysicsManager.hpp"
#include "RigidBody.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Physics {
class HitagiPhysicsManager : public IPhysicsManager {
public:
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    std::array<vec3f, 2> GetAABB(Asset::GeometryNode& node) final;
    void                 CreateRigidBody(Asset::GeometryNode& node) final;
    void                 DeleteRigidBody(Asset::GeometryNode& node) final;

    mat4f GetRigidBodyTransform(Asset::GeometryNode& node) final;
    void  UpdateRigidBodyTransform(Asset::GeometryNode& node) final;

    void ApplyCentralForce(Asset::GeometryNode& node, vec3f force) final;

private:
    std::unordered_map<std::string, RigidBody> m_RigidBodies;
};
}  // namespace Hitagi::Physics