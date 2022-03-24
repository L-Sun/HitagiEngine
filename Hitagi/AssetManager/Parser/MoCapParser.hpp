#pragma once
#include "../SceneNode.hpp"
#include "../Animation.hpp"
#include "Buffer.hpp"

namespace hitagi::asset {
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
    virtual ~MoCapParser() = default;

    virtual std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> Parse(const core::Buffer& buffer) = 0;
};

}  // namespace hitagi::asset
