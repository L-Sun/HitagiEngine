#include "Box.hpp"

using namespace My;

void Box::GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const {
    TransformAabb(m_vDimension, m_fMargin, trans, aabbMin, aabbMax);
}