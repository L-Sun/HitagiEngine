#pragma once
#include <vector>
#include "IRuntimeModule.hpp"
#include "SceneManager.hpp"

namespace My {

class IPhysicsManager : public IRuntimeModule {
public:
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

    virtual void CreateRigidBody(SceneGeometryNode& node, const SceneObjectGeometry& geometry) = 0;
    virtual void DeleteRigidBody(SceneGeometryNode& BaseSceneNode)                             = 0;

    virtual int  CreateRigidBodies() = 0;
    virtual void ClearRigidBodies()  = 0;

    virtual mat4f GetRigidBodyTransform(std::shared_ptr<void> rigidBody) = 0;
    virtual void  UpdateRigidBodyTransform(SceneGeometryNode& node)      = 0;

    virtual void ApplyCentralForce(std::shared_ptr<void> rigidBody, vec3f force) = 0;
};
extern std::unique_ptr<IPhysicsManager> g_hysicsManager;
}  // namespace My