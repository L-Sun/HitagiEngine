#pragma once
#include "../SceneNode.hpp"
#include "Buffer.hpp"

namespace Hitagi::Asset {
enum class MoCapFormat : unsigned {
    BVH,
    NUM_SUPPORT,
};
inline MoCapFormat get_mocap_format(std::string_view ext) {
    if (ext == ".bvh")
        return MoCapFormat::BVH;

    return MoCapFormat::NUM_SUPPORT;
}

class MoCapParser {
public:
    virtual std::shared_ptr<BoneNode> ParserSkeleton(const Core::Buffer& buffer) = 0;
    // TODO parser animation
    // virtual Animation ParseAnimation(const Core::Buffer& buffer, float sample_rate);
    virtual ~MoCapParser() = default;
};

}  // namespace Hitagi::Asset
