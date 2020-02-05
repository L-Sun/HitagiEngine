#pragma once
#include "Geometry.hpp"

namespace My {

class Sphere : public Geometry {
public:
    Sphere() = delete;
    Sphere(float radius) : Geometry(GeometryType::SPHERE), m_fRadius(radius) {}

    virtual void GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const;

    float GetRadius() const { return m_fRadius; }

protected:
    float m_fRadius;
};
}  // namespace My
