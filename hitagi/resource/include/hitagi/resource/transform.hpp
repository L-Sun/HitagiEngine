#pragma once
#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/entity.hpp>

namespace hitagi::resource {

struct Transform {
    Transform() = default;
    Transform(const math::mat4f& local_matrix);

    math::vec3f local_translation = math::vec3f{0.0f, 0.0f, 0.0f};
    math::quatf local_rotation    = math::quatf{0.0f, 0.0f, 0.0f, 1.0f};
    math::vec3f local_scaling     = math::vec3f{1.0f, 1.0f, 1.0f};

    math::mat4f world_matrix = math::mat4f::identity();

    inline math::vec3f GetPosition() const { return std::get<0>(decompose(world_matrix)); }
    inline math::quatf GetRotation() const { return std::get<1>(decompose(world_matrix)); }
    inline math::vec3f GetScale() const { return std::get<2>(decompose(world_matrix)); }
};

struct Hierarchy {
    ecs::Entity parentID = ecs::Entity::InvalidEntity();
};
}  // namespace hitagi::resource
