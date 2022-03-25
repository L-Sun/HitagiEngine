#pragma once
#include <Hitagi/Math/Matrix.hpp>

namespace hitagi::physics {

class MotionState {
public:
    MotionState(const math::mat4f& transition) : m_Transition(transition), m_CenterOfMassOffset(0.0f) {}
    MotionState(const math::mat4f& transition, const math::vec3f& centroid)
        : m_Transition(transition), m_CenterOfMassOffset(centroid) {}
    void        SetTransition(const math::mat4f& transition) { m_Transition = transition; }
    void        SetCenterOfMass(const math::vec3f& centroid) { m_CenterOfMassOffset = centroid; }
    const auto& GetTransition() const { return m_Transition; }
    const auto& GetCenterOfMassOffset() const { return m_CenterOfMassOffset; }

private:
    math::mat4f m_Transition;
    math::vec3f m_CenterOfMassOffset;
};
}  // namespace hitagi::physics
