#pragma once
#include <hitagi/resource/scene_node.hpp>
#include <hitagi/ecs/entity.hpp>

namespace hitagi::resource {
struct Armature {
    std::pmr::vector<ecs::Entity> bone_collection;
};
}  // namespace hitagi::resource