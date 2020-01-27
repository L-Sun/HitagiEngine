#include "Plane.hpp"

using namespace My;

void Plane::GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const {
    (void)trans;
    float minf = std::numeric_limits<float>::min();
    float maxf = std::numeric_limits<float>::max();
    aabbMin    = {minf, minf, minf};
    aabbMax    = {maxf, maxf, maxf};
}