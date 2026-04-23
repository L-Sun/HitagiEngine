module;

#include <spdlog/logger.h>

module asset;
import std;

namespace hitagi::asset {

namespace {

struct TransformHierarchyCache {
    ecs::Schedule* schedule = nullptr;

    bool hierarchy_dirty = true;
    bool collect_started = false;
    bool update_finished = false;

    std::pmr::vector<ecs::Entity> hierarchy_entities;
    std::pmr::vector<ecs::Entity> hierarchy_order;
};

std::unordered_map<ecs::Schedule*, std::weak_ptr<TransformHierarchyCache>> g_TransformHierarchyCaches;

auto GetTransformHierarchyCache(ecs::Schedule& schedule) -> std::shared_ptr<TransformHierarchyCache> {
    auto& weak_cache = g_TransformHierarchyCaches[&schedule];
    auto  cache      = weak_cache.lock();
    if (!cache) {
        cache           = std::make_shared<TransformHierarchyCache>();
        cache->schedule = &schedule;
        weak_cache      = cache;
    }
    return cache;
}

void RebuildHierarchyOrder(TransformHierarchyCache& cache) {
    cache.hierarchy_order.clear();

    std::pmr::unordered_set<ecs::Entity> visited;
    std::pmr::unordered_set<ecs::Entity> visiting;

    std::ranges::sort(cache.hierarchy_entities, {}, &ecs::Entity::GetId);

    const std::function<void(ecs::Entity)> append_subtree = [&](ecs::Entity entity) {
        if (!entity || visited.contains(entity) || visiting.contains(entity)) return;
        if (!entity.Has<RelationShip>()) return;

        visiting.emplace(entity);
        cache.hierarchy_order.emplace_back(entity);

        auto children = entity.Get<RelationShip>().GetChildren() | std::ranges::to<std::pmr::vector<ecs::Entity>>();
        std::ranges::sort(children, {}, &ecs::Entity::GetId);
        for (auto child : children) {
            append_subtree(child);
        }

        visiting.erase(entity);
        visited.emplace(entity);
    };

    for (auto entity : cache.hierarchy_entities) {
        if (!entity || !entity.Has<RelationShip>()) continue;
        const auto& relation_ship = entity.Get<RelationShip>();
        if (!relation_ship.parent || !relation_ship.parent.Has<RelationShip>()) {
            append_subtree(entity);
        }
    }

    for (auto entity : cache.hierarchy_entities) {
        append_subtree(entity);
    }

    cache.hierarchy_dirty = false;
}

}  // namespace

void RelationShipSystem::OnUpdate(ecs::Schedule& schedule) {
    auto cache = GetTransformHierarchyCache(schedule);

    schedule.Request("attach_parent", [cache](const ecs::Entity entity, RelationShip& relation_ship) {
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
            cache->hierarchy_dirty     = true;
        }
    });
}

void TransformSystem::OnUpdate(ecs::Schedule& schedule) {
    auto cache = GetTransformHierarchyCache(schedule);

    schedule.SetOrder("attach_parent", "collect_transform_hierarchy");
    schedule.SetOrder("collect_transform_hierarchy", "update_world_matrix_flat");

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
            "collect_transform_hierarchy",
            [cache](const ecs::Entity entity, const RelationShip&) {
                if (!cache->collect_started) {
                    cache->hierarchy_entities.clear();
                    cache->collect_started = true;
                    cache->update_finished = false;
                }
                cache->hierarchy_entities.emplace_back(entity);
            })
        .Request(
            "update_world_matrix_flat",
            [cache](Transform&) {
                if (cache->update_finished) return;

                if (cache->hierarchy_dirty) {
                    RebuildHierarchyOrder(*cache);
                }

                std::pmr::unordered_set<ecs::Entity> updated_entities;

                for (auto entity : cache->hierarchy_order) {
                    if (!entity || !entity.Has<Transform>() || !entity.Has<RelationShip>()) continue;

                    auto& relation_ship = entity.Get<RelationShip>();
                    auto& transform     = entity.Get<Transform>();
                    const bool parent_dirty =
                        relation_ship.parent &&
                        updated_entities.contains(relation_ship.parent);

                    if (!parent_dirty && !relation_ship.subtree_dirty) continue;

                    if (relation_ship.parent && relation_ship.parent.Has<Transform>()) {
                        transform.world_matrix = relation_ship.parent.Get<Transform>().world_matrix * transform.local_matrix;
                    } else {
                        transform.world_matrix = transform.local_matrix;
                    }

                    relation_ship.subtree_dirty = false;
                    updated_entities.emplace(entity);
                }

                cache->collect_started = false;
                cache->update_finished = true;
            });
}

}  // namespace hitagi::asset
