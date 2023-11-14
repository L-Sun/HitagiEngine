#pragma once
#include <hitagi/asset/scene_node.hpp>
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

    virtual std::array<math::vec3f, 2> GetAABB(asset::Geometry& node) = 0;

    virtual void CreateRigidBody(asset::Geometry& node) = 0;
    virtual void DeleteRigidBody(asset::Geometry& node) = 0;

    virtual math::mat4f GetRigidBodyTransform(asset::Geometry& node)    = 0;
    virtual void        UpdateRigidBodyTransform(asset::Geometry& node) = 0;

    virtual void ApplyCentralForce(asset::Geometry& node, math::vec3f force) = 0;
};

}  // namespace hitagi::physics
