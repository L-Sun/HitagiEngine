#pragma once
#include <vector>
#include "IRuntimeModule.hpp"
#include "SceneNode.hpp"

namespace Hitagi::Physics {

class IPhysicsManager : public IRuntimeModule {
public:
    int  Initialize() override = 0;
    void Finalize()   override = 0;
    void Tick()       override = 0;

    virtual std::array<vec3f, 2> GetAABB(Resource::SceneGeometryNode& node) = 0;

    virtual void CreateRigidBody(Resource::SceneGeometryNode& node) = 0;
    virtual void DeleteRigidBody(Resource::SceneGeometryNode& node) = 0;

    virtual mat4f GetRigidBodyTransform(Resource::SceneGeometryNode& node)    = 0;
    virtual void  UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) = 0;

    virtual void ApplyCentralForce(Resource::SceneGeometryNode& node, vec3f force) = 0;
};

}  // namespace Hitagi::Physics
namespace Hitagi {
extern std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager;
}