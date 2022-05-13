#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene_node.hpp>
#include <hitagi/resource/animation.hpp>

namespace hitagi::asset {
enum class MoCapFormat : unsigned {
    UNKWON,
    BVH,
};
inline MoCapFormat get_mocap_format(std::string_view ext) {
    if (ext == ".bvh")
        return MoCapFormat::BVH;

    return MoCapFormat::UNKWON;
}

class MoCapParser {
public:
    virtual ~MoCapParser() = default;

    virtual std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> Parse(const core::Buffer& buffer) = 0;
};

}  // namespace hitagi::asset
