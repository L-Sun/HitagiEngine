#pragma once
#include "aabb.hpp"

namespace My {
enum struct GeometryType {
    BOX,
    SPHERE,
    CYLINDER,
    CONE,
    PLANE,
    CAPSULE,
    TRIANGLE
};

class Geometry {
public:
    Geometry(GeometryType gemoetry_type) : m_kGeometryType(gemoetry_type) {}
    Geometry()          = default;
    virtual ~Geometry() = default;

    virtual void  GetAabb(const mat4& trans, vec3& aabbMin,
                          vec3& aabbMax) const = 0;
    virtual void  GetBoundingSphere(vec3& center, float& radius) const;
    virtual float GetAngularMotionDisc() const;

    void CalculateTemporalAabb(const mat4& curTrans, const vec3& linvel,
                               const vec3& angvel, float timeStep,
                               vec3& temporalAabbMin,
                               vec3& temporalAabbMax) const;

    GeometryType GetGeometryType() const { return m_kGeometryType; }

protected:
    GeometryType m_kGeometryType;
    float        m_fMargin = std::numeric_limits<float>::epsilon();
};

}  // namespace My
