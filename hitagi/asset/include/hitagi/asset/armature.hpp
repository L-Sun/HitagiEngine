#pragma once
#include <hitagi/asset/scene_node.hpp>

namespace hitagi::asset {
struct Bone {
    std::pmr::string name;

    std::pmr::vector<std::shared_ptr<Bone>> children;
    std::weak_ptr<Bone>                     parent;

    Transform transform;
};

struct Armature : public Resource {
    using Resource::Resource;

    std::pmr::vector<std::shared_ptr<Bone>> bone_collection;
};
using ArmatureNode = SceneNodeWithObject<Armature>;

}  // namespace hitagi::asset