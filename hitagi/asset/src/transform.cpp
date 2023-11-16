#include <hitagi/asset/transform.hpp>
#include "hitagi/ecs/entity_manager.hpp"

using namespace hitagi::math;

namespace hitagi::asset {
Transform::Transform(const mat4f& local_matrix)
    : local_translation(get_translation(local_matrix)),
      local_rotation(std::get<1>(decompose(local_matrix))),
      local_scaling(std::get<2>(decompose(local_matrix))) {
}

Transform::Transform(const Transform& other)
    : local_translation(other.local_translation),
      local_rotation(other.local_rotation),
      local_scaling(other.local_scaling) {
}

Transform& Transform::operator=(const Transform& other) {
    local_translation = other.local_translation;
    local_rotation    = other.local_rotation;
    local_scaling     = other.local_scaling;
    return *this;
}

void TransformSystem::OnUpdate(ecs::Schedule& schedule) {
    // Reset transform matrix
    schedule
        .Request("ResetLocal", [](LocalToParent& transform) {
            transform.value = math::mat4f::identity();
        })
        .Request("ResetWorld", [](LocalToWorld& transform) {
            transform.value = math::mat4f::identity();
        });

    schedule
        .Request(
            "Scale->LocalToWorld (No parent)",
            [](const Scale& scale, LocalToWorld& transform) {
                transform.value = math::scale(scale.value) * transform.value;
            },
            ecs::Filter().None<Parent>())
        .Request(
            "Rotation->LocalToWorld (No parent)",
            [](const Rotation& rotation, LocalToWorld& transform) {
                transform.value = math::rotate(rotation.euler) * transform.value;
            },
            ecs::Filter().None<Parent>())
        .Request(
            "Translation->LocalToWorld (No parent)",
            [](const Translation& translation, LocalToWorld& transform) {
                transform.value = math::rotate(translation.value) * transform.value;
            },
            ecs::Filter().None<Parent>());

    schedule
        .Request(
            "Scale->LocalToParent",
            [](const Scale& scale, LocalToParent& transform) {
                transform.value = math::scale(scale.value) * transform.value;
            },
            ecs::Filter().All<Parent>())
        .Request(
            "Rotation->LocalToParent",
            [](const Rotation& rotation, LocalToParent& transform) {
                transform.value = math::rotate(rotation.euler) * transform.value;
            },
            ecs::Filter().All<Parent>())
        .Request(
            "Translation->LocalToParent",
            [](const Translation& translation, LocalToParent& transform) {
                transform.value = math::rotate(translation.value) * transform.value;
            },
            ecs::Filter().All<Parent>());

    schedule
        .Request(
            "LocalToParent->LocalToWorld",
            [&](ecs::Entity root) {
                std::function<void(ecs::Entity, const math::mat4f&)> recursive_update =
                    [&, &entity_manager = schedule.world.GetEntityManager()](ecs::Entity entity, const math::mat4f& parent_transform) {
                        if (auto local_to_world = entity_manager.GetComponent<LocalToWorld>(entity); local_to_world.has_value()) {
                            math::mat4f& world_transform = local_to_world->get().value;

                            world_transform = parent_transform * world_transform;

                            if (auto children = entity_manager.GetComponent<Children>(entity); children.has_value()) {
                                for (auto child : children->get().values)
                                    recursive_update(child, world_transform);
                            }
                        }
                    };

                recursive_update(root, math::mat4f::identity());
            },
            ecs::Filter().None<Parent>().All<Children, LocalToWorld>());
}

}  // namespace hitagi::asset