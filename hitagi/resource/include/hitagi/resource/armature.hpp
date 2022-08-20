#pragma once
#include <hitagi/resource/scene_node.hpp>

namespace hitagi::resource {
struct Bone {
    std::pmr::string name;

    std::pmr::vector<std::shared_ptr<Bone>> children;
    std::weak_ptr<Bone>                     parent;

    Transform transform;
};

struct Armature {
    std::pmr::vector<std::shared_ptr<Bone>> bone_collection;
};
using ArmatureNode = SceneNodeWithObject<Armature>;

}  // namespace hitagi::resource