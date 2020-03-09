#include "Box.hpp"

namespace Hitagi::Physics {
void Box::GetAabb(const mat4f& trans, vec3f& aabbMin, vec3f& aabbMax) const {
    TransformAabb(m_Dimension, m_Margin, trans, aabbMin, aabbMax);
}
}  // namespace Hitagi::Physics