#pragma once
#include <hitagi/resource/scene_node.hpp>
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <vector>

namespace hitagi::physics {

class IPhysicsManager : public IRuntimeModule {
public:
    int  Initialize() override = 0;
    void Finalize() override   = 0;
    void Tick() override       = 0;

    virtual std::array<math::vec3f, 2> GetAABB(asset::GeometryNode& node) = 0;

    virtual void CreateRigidBody(asset::GeometryNode& node) = 0;
    virtual void DeleteRigidBody(asset::GeometryNode& node) = 0;

    virtual math::mat4f GetRigidBodyTransform(asset::GeometryNode& node)    = 0;
    virtual void        UpdateRigidBodyTransform(asset::GeometryNode& node) = 0;

    virtual void ApplyCentralForce(asset::GeometryNode& node, math::vec3f force) = 0;
};

}  // namespace hitagi::physics
namespace hitagi {
extern std::unique_ptr<physics::IPhysicsManager> g_PhysicsManager;
}