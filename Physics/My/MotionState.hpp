#pragma once
#include "geommath.hpp"

namespace My {

class MotionState {
public:
    MotionState(mat4 transition) : m_Transition(transition) {}
    void SetTransition(const mat4& transition) { m_Transition = transition; }
    const mat4& GetTransition() const { return m_Transition; }

private:
    mat4 m_Transition;
};
}  // namespace My
