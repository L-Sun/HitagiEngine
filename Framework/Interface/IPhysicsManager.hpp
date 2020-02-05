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

    virtual void CreateRigidBody(SceneGeometryNode&         node,
                                 const SceneObjectGeometry& geometry) = 0;
    virtual void DeleteRigidBody(SceneGeometryNode& BaseSceneNode)    = 0;

    virtual int  CreateRigidBodies() = 0;
    virtual void ClearRigidBodies()  = 0;

    virtual mat4 GetRigidBodyTransform(void* rigidBody)            = 0;
    virtual void UpdateRigidBodyTransform(SceneGeometryNode& node) = 0;

    virtual void ApplyCentralForce(void* rigidBody, vec3 force) = 0;
};
extern std::unique_ptr<IPhysicsManager> g_pPhysicsManager;
}  // namespace My