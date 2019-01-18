#pragma once
#include "geommath.hpp"

namespace My {
inline void TransformAabb(const vec3& halfExtents, float margin,
                          const mat4& trans, vec3& aabbMinOut,
                          vec3& aabbMaxOut) {
    vec3 halfExtentssWithMargin = halfExtents + vec3(margin);
    vec3 center, extent;
    center = GetOrigin(trans);
    mat3 basis;
    Shrink(basis, trans);
    basis      = Absolute(basis);
    extent     = halfExtentssWithMargin * basis;
    aabbMinOut = center - extent;
    aabbMaxOut = center + extent;
}
}  // namespace My
