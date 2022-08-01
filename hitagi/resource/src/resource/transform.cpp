#include <hitagi/resource/transform.hpp>

namespace hitagi::resource {
Transform::Transform(const math::mat4f& local_matrix) {
    auto&& [translation, rotation, scaling] = math::decompose(local_matrix);

    local_translation = translation;
    local_rotation    = rotation;
    local_scaling     = scaling;
}

}  // namespace hitagi::resource