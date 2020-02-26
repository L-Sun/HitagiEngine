#pragma once
#include "IPhysicsManager.hpp"
#include "Geometry.hpp"

namespace My {
class MyPhysicsManager : public IPhysicsManager {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void CreateRigidBody(SceneGeometryNode&         node,
                         const SceneObjectGeometry& geometry) final;
    void DeleteRigidBody(SceneGeometryNode& node) final;

    int  CreateRigidBodies() final;
    void ClearRigidBodies() final;

    mat4f GetRigidBodyTransform(std::shared_ptr<void> rigidBody) final;
    void  UpdateRigidBodyTransform(SceneGeometryNode& node) final;

    void ApplyCentralForce(std::shared_ptr<void> rigidBody, vec3f force) final;

#if defined(_DEBUG)
    void DrawDebugInfo();
#endif

protected:
#if defined(_DEBUG)
    void DrawAabb(const Geometry& geometry, const mat4f& trans,
                  const vec3f& centerOfMass);
#endif
};
}  // namespace My