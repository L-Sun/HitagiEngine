#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/math/transform.hpp>

namespace hitagi::asset {
struct Bone {
    std::pmr::string                        name;
    std::pmr::vector<std::shared_ptr<Bone>> children;
    std::weak_ptr<Bone>                     parent;

    math::mat4f offset_matrix;
};

struct Skeleton : public Resource {
    Skeleton(std::string_view name = "") : Resource(Type::Skeleton, name) {}

    std::pmr::vector<std::shared_ptr<Bone>> bones;
};

struct SkeletonComponent {
    std::shared_ptr<Skeleton> skeleton;
};

}  // namespace hitagi::asset