
#pragma once
#include "Geometry.hpp"

namespace My {

class Plane : public Geometry {
public:
    Plane() = delete;
    Plane(vec3 normal, float intercept) : Geometry(GeometryType::PLANE) {}

    void GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const final;

    vec3  GetNormal() const { return m_vNormal; };
    float GetIntercept() const { return m_fIntercept; };

protected:
    vec3  m_vNormal;
    float m_fIntercept;
};
}  // namespace My
