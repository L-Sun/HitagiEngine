#pragma once
#include "geommath.hpp"

namespace My {

class MotionState {
public:
    MotionState(mat4 transition)
        : m_Transition(transition), m_CenterOfMassOffset(0.0f) {}
    MotionState(const mat4& transition, const vec3& centroid)
        : m_Transition(transition), m_CenterOfMassOffset(centroid) {}
    void SetTransition(const mat4& transition) { m_Transition = transition; }
    void SetCenterOfMass(const vec3& centroid) {
        m_CenterOfMassOffset = centroid;
    }
    const mat4& GetTransition() const { return m_Transition; }
    const vec3& GetCenterOfMassOffset() const { return m_CenterOfMassOffset; }

private:
    mat4 m_Transition;
    vec3 m_CenterOfMassOffset;
};
}  // namespace My
