#include <hitagi/asset/transform.hpp>
#include <hitagi/ecs/filter.hpp>
#include <hitagi/utils/logger.hpp>

#include <spdlog/logger.h>

namespace hitagi::asset {

void RelationShipSystem::OnUpdate(ecs::Schedule& schedule) {
    schedule.Request("attach_parent", [](const ecs::Entity entity, RelationShip& relation_ship) {
        if (relation_ship.prev_parent != relation_ship.parent) {  // parent changed
            if (relation_ship.prev_parent) {
                auto& prev_parent = relation_ship.prev_parent.Get<RelationShip>();
                prev_parent.children.erase(entity);
            }
            if (relation_ship.parent) {
                auto& parent = relation_ship.parent.Get<RelationShip>();
                parent.children.insert(entity);
            }
            relation_ship.prev_parent = relation_ship.parent;
        }
    });
}

void TransformSystem::OnUpdate(ecs::Schedule& schedule) {
    schedule.SetOrder("attach_parent", "update_world_matrix_from_root");

    schedule
        .Request(
            "update_local_matrix",
            [](Transform& transform) {
                transform.world_matrix = transform.ToMatrix();
            })
        .Request(
            "update_world_matrix_from_root",
            [](const Transform& transform, const RelationShip& relation_ship) {
                const std::function<void(ecs::Entity, const math::mat4f)> recursive_update =
                    [&](ecs::Entity entity, const math::mat4f parent_transform) {
                        auto& transform        = entity.Get<Transform>();
                        transform.world_matrix = parent_transform * transform.world_matrix;
                        for (auto child : entity.Get<RelationShip>().GetChildren()) {
                            recursive_update(child, transform.world_matrix);
                        }
                    };

                // root entity
                if (!relation_ship.parent) {
                    for (auto entity : relation_ship.GetChildren()) {
                        recursive_update(entity, transform.world_matrix);
                    }
                }
            });
}

}  // namespace hitagi::asset