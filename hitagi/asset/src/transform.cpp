#include <hitagi/asset/transform.hpp>

using namespace hitagi::math;

namespace hitagi::asset {
Transform::Transform(const mat4f& local_matrix)
    : local_translation(get_translation(local_matrix)),
      local_rotation(std::get<1>(decompose(local_matrix))),
      local_scaling(std::get<2>(decompose(local_matrix))) {
}
}  // namespace hitagi::asset