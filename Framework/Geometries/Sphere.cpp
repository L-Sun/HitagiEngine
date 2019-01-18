#include "Sphere.hpp"

using namespace My;

void Sphere::GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const {
    vec3 center = GetOrigin(trans);
    vec3 extent(m_fMargin, m_fMargin, m_fMargin);
    aabbMin = center - extent;
    aabbMax = center + extent;
}