#pragma once
#include "Geometry.hpp"

namespace My {

class Sphere : public Geometry {
public:
    Sphere() = delete;
    Sphere(float radius) : Geometry(GeometryType::SPHERE), m_fRadius(radius) {}

    virtual void GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const;

    float GetRadius() const { return m_fRadius; }

protected:
    float m_fRadius;
};
}  // namespace My
