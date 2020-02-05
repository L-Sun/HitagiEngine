#pragma once
#include "geommath.hpp"

namespace My {
inline void TransformAabb(const vec3f& halfExtents, float margin,
                          const mat4f& trans, vec3f& aabbMinOut,
                          vec3f& aabbMaxOut) {
    vec3f halfExtentssWithMargin = halfExtents + vec3f(margin);
    vec3f center, extent;
    center = GetOrigin(trans);
    mat3f basis;
    Shrink(basis, trans);
    basis      = Absolute(basis);
    extent     = halfExtentssWithMargin * basis;
    aabbMinOut = center - extent;
    aabbMaxOut = center + extent;
}
}  // namespace My
