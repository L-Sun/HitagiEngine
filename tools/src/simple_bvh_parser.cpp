#include "simple_bvh_parser.hpp"
#include <exception>
#include <hitagi/math/transform.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
using namespace hitagi;
using namespace hitagi::math;

std::vector<std::string> Tokenizer(std::stringstream& ss);

std::optional<Animation> parse_bvh(const hitagi::core::Buffer& buffer) {
    std::stringstream ss;
    ss.write(buffer.Span<char>().data(), buffer.GetDataSize());

    auto tokens = Tokenizer(ss);

    Animation anima;

    std::shared_ptr<BoneNode> root;
    std::shared_ptr<BoneNode> current_node;

    std::vector<std::pair<decltype(root), std::vector<Channel>>> joints_channels;

    std::vector<decltype(root)> stack;
    size_t                      pos            = 0;
    size_t                      total_channels = 0;
    for (; pos < tokens.size(); pos++) {
        if (tokens[pos] == "HIERARCHY") {
            continue;
        }
        if (tokens[pos] == "ROOT") {
            current_node        = std::make_shared<BoneNode>();
            current_node->index = anima.joints.size();
            anima.joints.emplace_back(current_node);
            current_node->name = tokens[pos + 1];

            root = current_node;
        } else if (tokens[pos] == "JOINT") {
            auto parent         = current_node;
            current_node        = std::make_shared<BoneNode>();
            current_node->index = anima.joints.size();
            anima.joints.emplace_back(current_node);
            current_node->name = tokens[pos + 1];

            parent->children.emplace_back(current_node);
            current_node->parent = parent;
        } else if (tokens[pos] == "End Site") {
            auto parent         = current_node;
            current_node        = std::make_shared<BoneNode>();
            current_node->index = anima.joints.size();
            anima.joints.emplace_back(current_node);
            current_node->name = parent->name + "-End";

            parent->children.emplace_back(current_node);
            current_node->parent = parent;
        } else if (tokens[pos] == "OFFSET") {
            // ! Our engine is Z up, but bvh is Y up, so we need change the axis order here
            float y = std::stof(tokens[++pos]) * 0.01;
            float z = std::stof(tokens[++pos]) * 0.01;
            float x = std::stof(tokens[++pos]) * 0.01;

            current_node->offset    = hitagi::math::vec3f{x, y, z};
            current_node->transform = translate(current_node->transform, current_node->offset);
        }
        // ! the means of CHANNELS of .bvh format is not same as our engine
        else if (tokens[pos] == "CHANNELS") {
            size_t channels_count = std::stoi(tokens[pos + 1]);
            total_channels += channels_count;
            auto& [_, channels] = joints_channels.emplace_back(current_node, std::vector<Channel>{});

            for (size_t i = 0; i < channels_count; i++) {
                channels.emplace_back(channel_map.at(tokens[pos + 2 + i]));
            }
        } else if (tokens[pos] == "{") {
            stack.push_back(current_node);
        } else if (tokens[pos] == "}") {
            stack.pop_back();
            if (stack.empty()) break;
            current_node = stack.back();
        } else {
            continue;
        }
    }
    if (root != current_node) {
        fmt::print("Unexcept erro ocuur when import bvh");
        return std::nullopt;
    }

    size_t num_frames = 0;
    double frame_time = 0;
    while (pos < tokens.size()) {
        if (tokens[pos] == "Frames") {
            num_frames = std::stoi(tokens[pos + 1]);
        } else if (tokens[pos] == "Frame Time") {
            frame_time = std::stod(tokens[pos + 1]);
            pos += 2;
            break;
        }
        pos++;
    }

    if (num_frames == 0) {
        fmt::print("The bvh file does not have frame data!");
        return std::nullopt;
    }

    if ((tokens.size() - pos) / num_frames != total_channels) {
        fmt::print("the animation data is broken!");
        return std::nullopt;
    }

    anima.frame_rate = (1.0 / frame_time);

    for (size_t i = 0; i < num_frames; i++) {
        anima.frames.emplace_back(std::vector<TRS>(anima.joints.size()));

        for (auto&& [joint, channels] : joints_channels) {
            vec3f translation = joint->offset;
            mat4f rotation(1.0f);

            for (auto channel : channels) {
                if (pos == tokens.size()) {
                    fmt::print("the parsing of animation is terminated early!");
                    return std::nullopt;
                }
                float value = std::stof(tokens[pos]);
                switch (channel) {
                    // ! Our engine is Z up, but bvh is Y up, so we need change the axis order here
                    case Channel::Xposition:
                        translation.y += value * 0.01;
                        break;
                    case Channel::Yposition:
                        translation.z += value * 0.01;
                        break;
                    case Channel::Zposition:
                        translation.x += value * 0.01;
                        break;
                    // ! Also we unkown the rotation order, so use axis rotation here
                    case Channel::Xrotation:
                        rotation = rotation * rotate_y(mat4f(1.0f), radians(value));
                        break;
                    case Channel::Yrotation:
                        rotation = rotation * rotate_z(mat4f(1.0f), radians(value));
                        break;
                    case Channel::Zrotation:
                        rotation = rotation * rotate_x(mat4f(1.0f), radians(value));
                        break;
                    default:
                        break;
                }
                pos++;
            }
            auto [_t, _r, _s] = decompose(rotation);

            auto   iter        = std::find(std::begin(anima.joints), std::end(anima.joints), joint);
            size_t joint_index = std::distance(std::begin(anima.joints), iter);

            anima.frames[i][joint_index] = {
                .translation = translation,
                .rotation    = _r,
            };
        }
    }

    if (pos < tokens.size()) {
        fmt::print("there are some tokens did not handle after parsing: token is {}", tokens[pos]);
    }

    return anima;
}

std::vector<std::string> Tokenizer(std::stringstream& ss) {
    std::vector<std::string> result;
    std::string              token;

    while (ss >> token) {
        if (token == "End") {
            ss >> token;
            if (token == "Site") {
                result.emplace_back("End Site");
            }
        } else if (token == "Frames:") {
            result.emplace_back("Frames");
        } else if (token == "Frame") {
            ss >> token;
            if (token == "Time:")
                result.emplace_back("Frame Time");
        } else {
            result.emplace_back(token);
        }

        token = result.back();
    }
    return result;
}
