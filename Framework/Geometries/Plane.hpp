
#pragma once
#include "Geometry.hpp"

namespace Hitagi {

class Plane : public Geometry {
public:
    Plane() = delete;
    Plane(vec3f normal, float intercept) : Geometry(GeometryType::PLANE) {}

    void GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const final;

    vec3f GetNormal() const { return m_Normal; };
    float GetIntercept() const { return m_Intercept; };

protected:
    vec3f m_Normal;
    float m_Intercept;
};
}  // namespace Hitagi
