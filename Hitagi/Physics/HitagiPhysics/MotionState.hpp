#pragma once
#include "HitagiMath.hpp"

namespace Hitagi::Physics {

class MotionState {
public:
    MotionState(mat4f transition) : m_Transition(transition), m_CenterOfMassOffset(0.0f) {}
    MotionState(const mat4f& transition, const vec3f& centroid)
        : m_Transition(transition), m_CenterOfMassOffset(centroid) {}
    void         SetTransition(const mat4f& transition) { m_Transition = transition; }
    void         SetCenterOfMass(const vec3f& centroid) { m_CenterOfMassOffset = centroid; }
    const mat4f& GetTransition() const { return m_Transition; }
    const vec3f& GetCenterOfMassOffset() const { return m_CenterOfMassOffset; }

private:
    mat4f m_Transition;
    vec3f m_CenterOfMassOffset;
};
}  // namespace Hitagi::Physics
