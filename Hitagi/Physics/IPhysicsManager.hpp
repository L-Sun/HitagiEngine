#pragma once
#include "IRuntimeModule.hpp"
#include "SceneNode.hpp"

#include <Math/Vector.hpp>
#include <Math/Matrix.hpp>

#include <vector>

namespace Hitagi::Physics {

class IPhysicsManager : public IRuntimeModule {
public:
    int  Initialize() override = 0;
    void Finalize() override   = 0;
    void Tick() override       = 0;

    virtual std::array<Math::vec3f, 2> GetAABB(Asset::GeometryNode& node) = 0;

    virtual void CreateRigidBody(Asset::GeometryNode& node) = 0;
    virtual void DeleteRigidBody(Asset::GeometryNode& node) = 0;

    virtual Math::mat4f GetRigidBodyTransform(Asset::GeometryNode& node)    = 0;
    virtual void        UpdateRigidBodyTransform(Asset::GeometryNode& node) = 0;

    virtual void ApplyCentralForce(Asset::GeometryNode& node, Math::vec3f force) = 0;
};

}  // namespace Hitagi::Physics
namespace Hitagi {
extern std::unique_ptr<Physics::IPhysicsManager> g_PhysicsManager;
}