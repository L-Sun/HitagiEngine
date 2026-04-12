module;

#include <spdlog/logger.h>

module asset;
import std;

namespace hitagi::asset {

void RelationShipSystem::OnUpdate(ecs::Schedule& schedule) {
    schedule.Request("attach_parent", [](const ecs::Entity entity, RelationShip& relation_ship) {
        if (relation_ship.prev_parent != relation_ship.parent) {  // parent changed
            if (relation_ship.prev_parent) {
                auto& prev_parent = relation_ship.prev_parent.Get<RelationShip>();
                prev_parent.children.erase(entity);
                prev_parent.subtree_dirty = true;
                for (auto parent = prev_parent.parent; parent; parent = parent.Get<RelationShip>().parent) {
                    parent.Get<RelationShip>().subtree_dirty = true;
                }
            }
            if (relation_ship.parent) {
                auto& parent = relation_ship.parent.Get<RelationShip>();
                parent.children.insert(entity);
                parent.subtree_dirty = true;
                for (auto ancestor = parent.parent; ancestor; ancestor = ancestor.Get<RelationShip>().parent) {
                    ancestor.Get<RelationShip>().subtree_dirty = true;
                }
            }
            relation_ship.prev_parent = relation_ship.parent;
            relation_ship.subtree_dirty = true;
        }
    });
}

void TransformSystem::OnUpdate(ecs::Schedule& schedule) {
    schedule.SetOrder("attach_parent", "update_world_matrix_from_root");

    schedule
        .Request(
            "update_local_matrix",
            [](const ecs::Entity entity, Transform& transform, RelationShip& relation_ship) {
                if (transform.position == transform.cached_position &&
                    transform.rotation == transform.cached_rotation &&
                    transform.scaling == transform.cached_scaling) {
                    return;
                }

                transform.local_matrix    = transform.ToMatrix();
                transform.cached_position = transform.position;
                transform.cached_rotation = transform.rotation;
                transform.cached_scaling  = transform.scaling;

                relation_ship.subtree_dirty = true;
                for (auto parent = relation_ship.parent; parent; parent = parent.Get<RelationShip>().parent) {
                    parent.Get<RelationShip>().subtree_dirty = true;
                }
            })
        .Request(
            "update_world_matrix_from_root",
            [](Transform& transform, RelationShip& relation_ship) {
                const std::function<void(ecs::Entity, const math::mat4f, bool)> recursive_update =
                    [&](ecs::Entity entity, const math::mat4f parent_transform, bool parent_dirty) {
                        auto& child_relation = entity.Get<RelationShip>();
                        if (!parent_dirty && !child_relation.subtree_dirty) return;

                        auto& child_transform        = entity.Get<Transform>();
                        child_transform.world_matrix = parent_transform * child_transform.local_matrix;
                        const bool subtree_dirty     = parent_dirty || child_relation.subtree_dirty;
                        child_relation.subtree_dirty = false;

                        for (auto child : child_relation.GetChildren()) {
                            recursive_update(child, child_transform.world_matrix, subtree_dirty);
                        }
                    };

                // root entity
                if (!relation_ship.parent && relation_ship.subtree_dirty) {
                    transform.world_matrix     = transform.local_matrix;
                    relation_ship.subtree_dirty = false;
                    for (auto entity : relation_ship.GetChildren()) {
                        recursive_update(entity, transform.world_matrix, true);
                    }
                }
            });
}

}  // namespace hitagi::asset
