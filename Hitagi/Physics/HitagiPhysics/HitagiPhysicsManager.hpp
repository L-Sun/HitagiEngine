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

    std::array<vec3f, 2> GetAABB(Asset::SceneGeometryNode& node) final;
    void                 CreateRigidBody(Asset::SceneGeometryNode& node) final;
    void                 DeleteRigidBody(Asset::SceneGeometryNode& node) final;

    mat4f GetRigidBodyTransform(Asset::SceneGeometryNode& node) final;
    void  UpdateRigidBodyTransform(Asset::SceneGeometryNode& node) final;

    void ApplyCentralForce(Asset::SceneGeometryNode& node, vec3f force) final;

private:
#if defined(_DEBUG)
    void DrawAabb(const Geometry& geometry, const mat4f& trans, const vec3f& centerOfMass);
#endif

    std::unordered_map<std::string, RigidBody> m_RigidBodies;
};
}  // namespace Hitagi::Physics