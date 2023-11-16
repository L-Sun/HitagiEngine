#pragma once
#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/schedule.hpp>

namespace hitagi::asset {

struct Transform {
    Transform() = default;
    Transform(const math::mat4f& local_matrix);
    Transform(const Transform& other);
    Transform& operator=(const Transform& other);

    math::vec3f local_translation = math::vec3f{0.0f, 0.0f, 0.0f};
    math::quatf local_rotation    = math::quatf{0.0f, 0.0f, 0.0f, 1.0f};
    math::vec3f local_scaling     = math::vec3f{1.0f, 1.0f, 1.0f};

    math::mat4f world_matrix = math::mat4f::identity();

    inline math::mat4f GetLocalMatrix() const { return math::translate(local_translation) * math::rotate(local_rotation) * math::scale(local_scaling); }
    inline math::vec3f GetPosition() const { return math::get_translation(world_matrix); }
    inline math::quatf GetRotation() const { return math::get_rotation(world_matrix); }
    inline math::vec3f GetScale() const { return math::get_scaling(world_matrix); }
};

struct Parent {
    ecs::Entity value;
};

struct Children {
    std::pmr::vector<ecs::Entity> values;
};

struct LocalToWorld {
    math::mat4f value;
};

struct LocalToParent {
    math::mat4f value;
};

struct Translation {
    math::vec3f value;
};
struct Rotation {
    math::vec3f euler;
};
struct Scale {
    math::vec3f value;
};

struct TransformSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset
