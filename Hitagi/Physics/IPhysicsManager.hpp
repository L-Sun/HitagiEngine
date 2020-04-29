#pragma once
#include <vector>

#include "SceneManager.hpp"

namespace Hitagi::Physics {

class IPhysicsManager : public IRuntimeModule {
public:
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

    virtual void GetBoundingBox(vec3f& aabbMin, vec3f& aabbMax, const Resource::SceneObjectGeometry& geometry,
                                size_t lod = 0)                                                                    = 0;
    virtual void CreateRigidBody(Resource::SceneGeometryNode& node, const Resource::SceneObjectGeometry& geometry) = 0;
    virtual void DeleteRigidBody(Resource::SceneGeometryNode& BaseSceneNode)                                       = 0;

    virtual int  CreateRigidBodies() = 0;
    virtual void ClearRigidBodies()  = 0;

    virtual mat4f GetRigidBodyTransform(std::shared_ptr<void> rigidBody)      = 0;
    virtual void  UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) = 0;

    virtual void ApplyCentralForce(std::shared_ptr<void> rigidBody, vec3f force) = 0;
};
}  // namespace Hitagi::Physics
namespace Hitagi {
extern std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager;
}