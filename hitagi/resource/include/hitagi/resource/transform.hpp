#pragma once
#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/ecs/world.hpp>

#include <unordered_set>

namespace hitagi::resource {

struct Transform {
    Transform() = default;
    Transform(const math::mat4f& local_matrix);

    math::vec3f local_translation = math::vec3f{0.0f, 0.0f, 0.0f};
    math::quatf local_rotation    = math::quatf{0.0f, 0.0f, 0.0f, 1.0f};
    math::vec3f local_scaling     = math::vec3f{1.0f, 1.0f, 1.0f};

    math::mat4f world_matrix = math::mat4f::identity();

    inline math::mat4f GetLocalMatrix() const { return math::translate(local_translation) * math::rotate(local_rotation) * math::scale(local_scaling); }
    inline math::vec3f GetPosition() const { return math::get_translation(world_matrix); }
    inline math::quatf GetRotation() const { return math::get_rotation(world_matrix); }
    inline math::vec3f GetScale() const { return math::get_scaling(world_matrix); }
};

struct TransformSystem {
    static bool enable;
    static void update_world_matrix(ecs::Entity node, const std::shared_ptr<std::pmr::unordered_set<ecs::Entity>>& updated_set, ecs::World& world);
    static void OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta);
};
}  // namespace hitagi::resource
