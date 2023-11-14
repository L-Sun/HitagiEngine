#pragma once
#include <hitagi/asset/scene_node.hpp>

namespace hitagi::asset {
struct Bone {
    std::pmr::string                        name;
    std::pmr::vector<std::shared_ptr<Bone>> children;
    std::weak_ptr<Bone>                     parent;

    math::mat4f offset_matrix;
    Transform   transform;
};

struct Skeleton : public Resource {
    Skeleton(std::string_view name = "") : Resource(Type::Skeleton, name) {}

    std::pmr::vector<std::shared_ptr<Bone>> bones;
};

using SkeletonNode = SceneNodeWithObject<Skeleton>;

}  // namespace hitagi::asset