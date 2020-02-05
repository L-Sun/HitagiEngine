#include "Sphere.hpp"

using namespace My;

void Sphere::GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const {
    vec3f center = GetOrigin(trans);
    vec3f extent(m_fMargin, m_fMargin, m_fMargin);
    aabbMin = center - extent;
    aabbMax = center + extent;
}