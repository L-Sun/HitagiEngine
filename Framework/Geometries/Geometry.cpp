#include "Geometry.hpp"

using namespace My;

void Geometry::CalculateTemporalAabb(const mat4& curTrans, const vec3& linvel,
                                     const vec3& angvel, float timeStep,
                                     vec3& temporalAabbMin,
                                     vec3& temporalAabbMax) const {
    GetAabb(curTrans, temporalAabbMin, temporalAabbMax);

    vec3 linMotion = linvel * timeStep;
    temporalAabbMax += Max(linMotion, 0.0f);
    temporalAabbMin += Min(linMotion, 0.0f);

    float angularMotion = Length(angvel) * GetAngularMotionDisc() * timeStep;
    vec3  angularMotion3d(angularMotion, angularMotion, angularMotion);

    temporalAabbMin = temporalAabbMin - angularMotion3d;
    temporalAabbMax = temporalAabbMax + angularMotion3d;
}

void Geometry::GetBoundingSphere(vec3& center, float& radius) const {
    mat4 tran(1.0f);
    vec3 aabbMin, aabbMax;

    GetAabb(tran, aabbMin, aabbMax);

    radius = Length(aabbMax - aabbMin) * 0.5f;
    center = (aabbMin + aabbMax) * 0.5f;
}

float Geometry::GetAngularMotionDisc() const {
    vec3  center;
    float disc = 0.0f;
    GetBoundingSphere(center, disc);
    disc += Length(center);
    return disc;
}