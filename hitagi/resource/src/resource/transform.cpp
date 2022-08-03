#include <hitagi/resource/transform.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/scene.hpp>

#include <memory>

using namespace hitagi::math;

namespace hitagi::resource {
Transform::Transform(const math::mat4f& local_matrix)
    : local_translation(std::get<0>(decompose(local_matrix))),
      local_rotation(std::get<1>(decompose(local_matrix))),
      local_scaling(std::get<2>(decompose(local_matrix))) {
}

void TransformSystem::update_world_matrix(ecs::Entity node, const std::shared_ptr<std::pmr::unordered_set<ecs::Entity>>& updated_set, ecs::World& world) {
    if (updated_set->contains(node)) return;

    auto transform = world.AccessEntity<Transform>(node);
    if (!transform.has_value()) return;

    auto parent = world.AccessEntity<Hierarchy>(node);
    if (!parent.has_value() || parent->get().parentID == ecs::Entity::InvalidEntity()) {
        transform->get().world_matrix = transform->get().GetLocalMatrix();
        updated_set->emplace(node);
        return;
    }

    if (!updated_set->contains(parent->get().parentID)) {
        update_world_matrix(parent->get().parentID, updated_set, world);
    }
    auto parent_transform = world.AccessEntity<Transform>(parent->get().parentID);
    if (parent_transform.has_value()) {
        transform->get().world_matrix = parent_transform->get().world_matrix * transform->get().GetLocalMatrix();
    } else {
        transform->get().world_matrix = transform->get().GetLocalMatrix();
    }
    updated_set->emplace(node);
}

bool TransformSystem::enable = true;

void TransformSystem::OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta) {
    // Extend life cycle
    auto updated = std::make_shared<std::pmr::unordered_set<ecs::Entity>>();
    schedule
        // .RequestOnce("Transform", [&]() {
        //     std::pmr::vector<ecs::Entity> nodes;
        //     for (const auto& arche : schedule.world.GetArchetypes<resource::Hierarchy>()) {
        //         for (auto entity : arche->GetComponentArray<ecs::Entity>()) {
        //             nodes.emplace_back(entity);
        //         }
        //     }
        //     std::sort(nodes.begin(), nodes.end());
        //     for (auto node : nodes) {
        //         auto transform = schedule.world.AccessEntity<resource::Transform>(node);
        //         if (!transform.has_value()) continue;

        //         auto parent = schedule.world.AccessEntity<resource::Hierarchy>(node);
        //         if (parent.has_value()) {
        //             if (updated->contains(parent->get().parentID)) {
        //                 auto parent_transform         = schedule.world.AccessEntity<resource::Transform>(parent->get().parentID)->get();
        //                 transform->get().world_matrix = parent_transform.world_matrix * transform->get().GetLocalMatrix();
        //             } else {
        //                 assert(false);
        //             }
        //         } else {
        //             transform->get().world_matrix = transform->get().GetLocalMatrix();
        //             updated->emplace(node);
        //         }
        //     }
        // })
        // TODO Filer
        .Request("Transform-Hierachy", [&, updated](ecs::Entity& entity, Transform& transform) {
            update_world_matrix(entity, updated, schedule.world);
        })
        .Request("Transform Camera", [&, updated](ecs::Entity& entity, Transform& transform, Camera& camera) {
            camera.ApplyTransform(transform);
        });
}
}  // namespace hitagi::resource