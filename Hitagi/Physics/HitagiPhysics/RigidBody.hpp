#pragma once
#include <memory>
#include <utility>

#include <utility>

#include "HitagiMath.hpp"
#include "MotionState.hpp"

namespace Hitagi::Physics {

class RigidBody {
public:
    RigidBody(std::shared_ptr<Geometry> colllision_shape, std::shared_ptr<MotionState> state)
        : m_CollisionShape(std::move(colllision_shape)), m_MotionState(std::move(state)) {}

    std::shared_ptr<Geometry>    GetCollisionShape() const { return m_CollisionShape; }
    std::shared_ptr<MotionState> GetMotionState() const { return m_MotionState; }

private:
    std::shared_ptr<Geometry>    m_CollisionShape;
    std::shared_ptr<MotionState> m_MotionState;
};
}  // namespace Hitagi::Physics