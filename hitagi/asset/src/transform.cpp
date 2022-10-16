#include <hitagi/asset/transform.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/scene.hpp>

#include <memory>

using namespace hitagi::math;

namespace hitagi::asset {
Transform::Transform(const math::mat4f& local_matrix)
    : local_translation(std::get<0>(decompose(local_matrix))),
      local_rotation(std::get<1>(decompose(local_matrix))),
      local_scaling(std::get<2>(decompose(local_matrix))) {
}
}  // namespace hitagi::asset