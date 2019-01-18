#pragma once
#include <memory>
#include "Geometry.hpp"
#include "MotionState.hpp"

namespace My {

class RigidBody {
public:
    RigidBody(std::shared_ptr<Geometry>    colllisionShape,
              std::shared_ptr<MotionState> state)
        : m_pCollisionShape(colllisionShape), m_pMotionState(state) {}

    std::shared_ptr<Geometry> GetCollisionShape() const {
        return m_pCollisionShape;
    }
    std::shared_ptr<MotionState> GetMotionState() const {
        return m_pMotionState;
    }

private:
    std::shared_ptr<Geometry>    m_pCollisionShape;
    std::shared_ptr<MotionState> m_pMotionState;
};
}  // namespace My