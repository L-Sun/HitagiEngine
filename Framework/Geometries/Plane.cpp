#include "Plane.hpp"

using namespace My;
using namespace std;

void Plane::GetAabb(const mat4& trans, vec3& aabbMin, vec3& aabbMax) const {
    (void)trans;
    float minf = numeric_limits<float>::min();
    float maxf = numeric_limits<float>::max();
    aabbMin    = {minf, minf, minf};
    aabbMax    = {maxf, maxf, maxf};
}