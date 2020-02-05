#include "Box.hpp"

using namespace My;

void Box::GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const {
    TransformAabb(m_vDimension, m_fMargin, trans, aabbMin, aabbMax);
}