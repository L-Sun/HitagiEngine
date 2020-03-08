#include "Plane.hpp"

using namespace Hitagi;

void Plane::GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const {
    (void)trans;
    float minf = std::numeric_limits<float>::min();
    float maxf = std::numeric_limits<float>::max();
    aabbMin    = {minf, minf, minf};
    aabbMax    = {maxf, maxf, maxf};
}