#pragma once
#include <memory>
#include "Geometry.hpp"
#include "MotionState.hpp"

namespace Hitagi::Physics {

class RigidBody {
public:
    RigidBody(std::shared_ptr<Geometry> colllisionShape, std::shared_ptr<MotionState> state)
        : m_CollisionShape(colllisionShape), m_MotionState(state) {}

    std::shared_ptr<Geometry>    GetCollisionShape() const { return m_CollisionShape; }
    std::shared_ptr<MotionState> GetMotionState() const { return m_MotionState; }

private:
    std::shared_ptr<Geometry>    m_CollisionShape;
    std::shared_ptr<MotionState> m_MotionState;
};
}  // namespace Hitagi