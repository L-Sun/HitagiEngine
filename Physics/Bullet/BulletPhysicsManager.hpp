#pragma once
#include "IRuntimeModule.hpp"
#define BT_USE_DOUBLE_PRECISION 1
#include "btBulletDynamicsCommon.h"
#include "IPhysicsManager.hpp"
#include "SceneManager.hpp"

namespace My {
class BulletPhysicsManager : implements IPhysicsManager {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    virtual void CreateRigidBody(SceneGeometryNode&         node,
                                 const SceneObjectGeometry& geometry);
    virtual void DeleteRigidBody(SceneGeometryNode& node);

    virtual int  CreateRigidBodies();
    virtual void ClearRigidBodies();

    mat4 GetRigidBodyTransform(void* rigidBody);
    void UpdateRigidBodyTransform(SceneGeometryNode& node);

    void ApplyCentralForce(void* rigidBody, vec3 force);

protected:
    btBroadphaseInterface*               m_btBroadphase;
    btDefaultCollisionConfiguration*     m_btCollisionConfiguration;
    btCollisionDispatcher*               m_btDispatcher;
    btSequentialImpulseConstraintSolver* m_btSolver;
    btDiscreteDynamicsWorld*             m_btDynamicsWorld;

    std::vector<btCollisionShape*> m_btCollisionShapes;
};

}  // namespace My
