#pragma once
#include <hitagi/math/transform.hpp>
#include <hitagi/core/buffer.hpp>

#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>

struct TRS {
    hitagi::math::vec3f translation = {0.f, 0.f, 0.f};
    hitagi::math::quatf rotation    = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct BoneNode {
    std::string                            name;
    std::weak_ptr<BoneNode>                parent;
    std::vector<std::shared_ptr<BoneNode>> children;
    hitagi::math::vec3f                    offset;
    size_t                                 index;

    hitagi::math::mat4f transform{1.0f};
};

struct Animation {
    float                                  frame_rate;
    std::vector<std::vector<TRS>>          frames;
    std::vector<std::shared_ptr<BoneNode>> joints;
};

enum struct Channel : uint8_t {
    Xposition,
    Yposition,
    Zposition,
    Xrotation,
    Yrotation,
    Zrotation,
};

static const std::unordered_map<std::string, Channel> channel_map = {
    {"Xposition", Channel::Xposition},
    {"Yposition", Channel::Yposition},
    {"Zposition", Channel::Zposition},
    {"Xrotation", Channel::Xrotation},
    {"Yrotation", Channel::Yrotation},
    {"Zrotation", Channel::Zrotation},
};

std::optional<Animation> parse_bvh(const hitagi::core::Buffer& buffer);