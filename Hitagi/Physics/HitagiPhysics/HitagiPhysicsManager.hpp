#pragma once
#include "IPhysicsManager.hpp"
#include "RigidBody.hpp"
#include "HitagiMath.hpp"

namespace Hitagi::Physics {
class HitagiPhysicsManager : public IPhysicsManager {
public:
public:
    virtual ~HitagiPhysicsManager() {}

    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    std::array<vec3f, 2> GetAABB(Resource::SceneGeometryNode& node) final;
    void                 CreateRigidBody(Resource::SceneGeometryNode& node) final;
    void                 DeleteRigidBody(Resource::SceneGeometryNode& node) final;

    mat4f GetRigidBodyTransform(Resource::SceneGeometryNode& node) final;
    void  UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) final;

    void ApplyCentralForce(Resource::SceneGeometryNode& node, vec3f force) final;

#if defined(_DEBUG)
    void DrawDebugInfo();
#endif

private:
#if defined(_DEBUG)
    void DrawAabb(const Geometry& geometry, const mat4f& trans, const vec3f& centerOfMass);
#endif

    std::unordered_map<std::string, RigidBody> m_RigidBodies;
};
}  // namespace Hitagi::Physics