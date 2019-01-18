#pragma once
#include "Geometry.hpp"

namespace My {
class Box : public Geometry {
public:
    Box() : Geometry(GeometryType::BOX) {}
    Box(vec3 dimension)
        : Geometry(GeometryType::BOX), m_vDimension(dimension) {}

    virtual void GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const;

    vec3 GetDimension() const { return m_vDimension; }
    vec3 GetDimensionWithMargin() const { return m_vDimension + m_fMargin; }

protected:
    vec3 m_vDimension;
};
}  // namespace My
