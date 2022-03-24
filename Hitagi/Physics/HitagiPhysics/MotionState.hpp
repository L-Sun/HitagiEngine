#pragma once
#include <Math/Matrix.hpp>

namespace Hitagi::Physics {

class MotionState {
public:
    MotionState(const Math::mat4f& transition) : m_Transition(transition), m_CenterOfMassOffset(0.0f) {}
    MotionState(const Math::mat4f& transition, const Math::vec3f& centroid)
        : m_Transition(transition), m_CenterOfMassOffset(centroid) {}
    void        SetTransition(const Math::mat4f& transition) { m_Transition = transition; }
    void        SetCenterOfMass(const Math::vec3f& centroid) { m_CenterOfMassOffset = centroid; }
    const auto& GetTransition() const { return m_Transition; }
    const auto& GetCenterOfMassOffset() const { return m_CenterOfMassOffset; }

private:
    Math::mat4f m_Transition;
    Math::vec3f m_CenterOfMassOffset;
};
}  // namespace Hitagi::Physics
