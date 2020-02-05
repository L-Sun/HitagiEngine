
#pragma once
#include "Geometry.hpp"

namespace My {

class Plane : public Geometry {
public:
    Plane() = delete;
    Plane(vec3f normal, float intercept) : Geometry(GeometryType::PLANE) {}

    void GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const final;

    vec3f  GetNormal() const { return m_vNormal; };
    float GetIntercept() const { return m_fIntercept; };

protected:
    vec3f  m_vNormal;
    float m_fIntercept;
};
}  // namespace My
