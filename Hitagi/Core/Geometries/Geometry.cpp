#include "Geometry.hpp"

namespace Hitagi::Physics {

void Geometry::CalculateTemporalAabb(const mat4f& curTrans, const vec3f& linvel, const vec3f& angvel, float timeStep,
                                     vec3f& temporalAabbMin, vec3f& temporalAabbMax) const {
    GetAabb(curTrans, temporalAabbMin, temporalAabbMax);

    vec3f linMotion = linvel * timeStep;
    temporalAabbMax += Max(linMotion, 0.0f);
    temporalAabbMin += Min(linMotion, 0.0f);

    float angularMotion = angvel.norm() * GetAngularMotionDisc() * timeStep;
    vec3f angularMotion3d(angularMotion, angularMotion, angularMotion);

    temporalAabbMin = temporalAabbMin - angularMotion3d;
    temporalAabbMax = temporalAabbMax + angularMotion3d;
}

void Geometry::GetBoundingSphere(vec3f& center, float& radius) const {
    mat4f tran(1.0f);
    vec3f aabbMin, aabbMax;

    GetAabb(tran, aabbMin, aabbMax);

    radius = (aabbMax - aabbMin).norm() * 0.5f;
    center = (aabbMin + aabbMax) * 0.5f;
}

float Geometry::GetAngularMotionDisc() const {
    vec3f center;
    float disc = 0.0f;
    GetBoundingSphere(center, disc);
    disc += center.norm();
    return disc;
}
}  // namespace Hitagi::Physics