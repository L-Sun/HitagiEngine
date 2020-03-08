#pragma once
#include "aabb.hpp"

namespace Hitagi {
enum struct GeometryType { BOX, SPHERE, CYLINDER, CONE, PLANE, CAPSULE, TRIANGLE };

class Geometry {
public:
    Geometry(GeometryType gemoetryType) : m_GeometryType(gemoetryType) {}
    Geometry()          = default;
    virtual ~Geometry() = default;

    virtual void  GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const = 0;
    virtual void  GetBoundingSphere(vec3f& center, float& radius) const;
    virtual float GetAngularMotionDisc() const;

    void CalculateTemporalAabb(const mat4f& curTrans, const vec3f& linvel, const vec3f& angvel, float timeStep,
                               vec3f& temporalAabbMin, vec3f& temporalAabbMax) const;

    GeometryType GetGeometryType() const { return m_GeometryType; }

protected:
    GeometryType m_GeometryType;
    float        m_Margin = std::numeric_limits<float>::epsilon();
};

}  // namespace Hitagi
