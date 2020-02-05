#pragma once
#include "Geometry.hpp"

namespace My {
class Box : public Geometry {
public:
    Box() : Geometry(GeometryType::BOX) {}
    Box(vec3f dimension)
        : Geometry(GeometryType::BOX), m_vDimension(dimension) {}

    virtual void GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const;

    vec3f GetDimension() const { return m_vDimension; }
    vec3f GetDimensionWithMargin() const { return m_vDimension + m_fMargin; }

protected:
    vec3f m_vDimension;
};
}  // namespace My
