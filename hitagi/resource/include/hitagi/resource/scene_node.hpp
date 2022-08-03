#pragma once
#include <hitagi/resource/transform.hpp>

#include <vector>
#include <memory>

namespace hitagi::resource {
struct SceneNode {
    ecs::Entity entity;

    std::pmr::vector<std::shared_ptr<SceneNode>> children;
    std::weak_ptr<SceneNode>                     parent;

    Transform* transform = nullptr;
};
}  // namespace hitagi::resource
