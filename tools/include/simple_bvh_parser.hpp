#pragma once
#include <hitagi/math/transform.hpp>
#include <hitagi/core/buffer.hpp>

#include <memory>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>

struct TRS {
    hitagi::math::vec3f translation = {0.0f, 0.0f, 0.0f};
    hitagi::math::quatf rotation    = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct BoneNode {
    std::pmr::string                            name;
    std::weak_ptr<BoneNode>                     parent;
    std::pmr::vector<std::shared_ptr<BoneNode>> children;
    hitagi::math::vec3f                         offset;
    size_t                                      index;

    hitagi::math::mat4f transform = hitagi::math::mat4f::identity();
};

struct Animation {
    float                                       frame_rate;
    std::pmr::vector<std::pmr::vector<TRS>>     frames;
    std::pmr::vector<std::shared_ptr<BoneNode>> joints;
};

enum struct Channel : uint8_t {
    Xposition,
    Yposition,
    Zposition,
    Xrotation,
    Yrotation,
    Zrotation,
};

static const std::pmr::unordered_map<std::pmr::string, Channel> channel_map = {
    {"Xposition", Channel::Xposition},
    {"Yposition", Channel::Yposition},
    {"Zposition", Channel::Zposition},
    {"Xrotation", Channel::Xrotation},
    {"Yrotation", Channel::Yrotation},
    {"Zrotation", Channel::Zrotation},
};

std::optional<Animation> parse_bvh(const hitagi::core::Buffer& buffer, float metric_sacle = 1.0);