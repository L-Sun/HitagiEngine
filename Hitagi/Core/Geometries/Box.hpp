#pragma once
#include "Geometry.hpp"

namespace Hitagi::Physics {
class Box : public Geometry {
public:
    Box() : Geometry(GeometryType::BOX) {}
    Box(const vec3f& dimension) : Geometry(GeometryType::BOX), m_Dimension(dimension) {}

    virtual void GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const;

    vec3f GetDimension() const { return m_Dimension; }
    vec3f GetDimensionWithMargin() const { return m_Dimension + m_Margin; }

protected:
    vec3f m_Dimension;
};
}  // namespace Hitagi::Physics
