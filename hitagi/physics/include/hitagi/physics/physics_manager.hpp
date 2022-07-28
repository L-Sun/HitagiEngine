#pragma once
#include <hitagi/resource/scene_node.hpp>
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <vector>

namespace hitagi::physics {

class IPhysicsManager : public RuntimeModule {
public:
    bool Initialize() override = 0;
    void Finalize() override   = 0;
    void Tick() override       = 0;

    virtual std::array<math::vec3f, 2> GetAABB(resource::Geometry& node) = 0;

    virtual void CreateRigidBody(resource::Geometry& node) = 0;
    virtual void DeleteRigidBody(resource::Geometry& node) = 0;

    virtual math::mat4f GetRigidBodyTransform(resource::Geometry& node)    = 0;
    virtual void        UpdateRigidBodyTransform(resource::Geometry& node) = 0;

    virtual void ApplyCentralForce(resource::Geometry& node, math::vec3f force) = 0;
};

}  // namespace hitagi::physics
namespace hitagi {
extern std::unique_ptr<physics::IPhysicsManager> g_PhysicsManager;
}