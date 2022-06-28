#include <hitagi/parser/bvh.hpp>

#include <spdlog/spdlog.h>

#include <sstream>
#include <unordered_map>
#include <ranges>

using namespace hitagi::math;

namespace hitagi::asset {
const std::unordered_map<std::string, BvhParser::Channel> BvhParser::m_ChannelMap = {
    {"Xposition", Channel::Xposition},
    {"Yposition", Channel::Yposition},
    {"Zposition", Channel::Zposition},
    {"Xrotation", Channel::Xrotation},
    {"Yrotation", Channel::Yrotation},
    {"Zrotation", Channel::Zrotation},
};

std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> BvhParser::Parse(const core::Buffer& buffer) {
    auto logger = spdlog::get("AssetManager");

    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());

    auto tokens = Tokenizer(ss);

    AnimationBuilder anima_builder;

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
            current_node = std::make_shared<BoneNode>(tokens[pos + 1]);
            root         = current_node;
        } else if (tokens[pos] == "JOINT") {
            auto parent  = current_node;
            current_node = std::make_shared<BoneNode>(tokens[pos + 1]);

            parent->AppendChild(current_node);
            current_node->SetParent(parent);
        } else if (tokens[pos] == "End Site") {
            auto parent  = current_node;
            current_node = std::make_shared<BoneNode>(parent->GetName() + "-End");

            parent->AppendChild(current_node);
            current_node->SetParent(parent);
        } else if (tokens[pos] == "OFFSET") {
            // ! Our engine is Z up, but bvh is Y up, so we need change the axis order here
            float y = std::stof(tokens[++pos]) * 0.01;
            float z = std::stof(tokens[++pos]) * 0.01;
            float x = std::stof(tokens[++pos]) * 0.01;

            current_node->Translate(vec3f{x, y, z});
        }
        // ! the means of CHANNELS of .bvh format is not same as our engine
        else if (tokens[pos] == "CHANNELS") {
            size_t channels_count = std::stoi(tokens[pos + 1]);
            total_channels += channels_count;
            auto& [_, channels] = joints_channels.emplace_back(current_node, std::vector<Channel>{});

            for (size_t i = 0; i < channels_count; i++) {
                channels.emplace_back(m_ChannelMap.at(tokens[pos + 2 + i]));
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
        logger->warn("Unexcept erro ocuur when import bvh");
        return {nullptr, nullptr};
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
        logger->warn("The bvh file does not have frame data!");
        return {root, nullptr};
    }

    if ((tokens.size() - pos) / num_frames != total_channels) {
        logger->warn("the animation data is broken!");
        return {root, nullptr};
    }

    anima_builder.SetFrameRate(1.0 / frame_time).SetSkeleton(root);

    for (size_t i = 0; i < num_frames; i++) {
        anima_builder.NewFrame();

        for (auto&& [joint, channels] : joints_channels) {
            vec3f translation = joint->GetPosition();
            mat4f rotation(1.0f);

            for (auto channel : channels) {
                if (pos == tokens.size()) {
                    logger->warn("the parsing of animation is terminated early!");
                    return {root, nullptr};
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
                        rotation = rotation * rotate_y(mat4f(1.0f), deg2rad(value));
                        break;
                    case Channel::Yrotation:
                        rotation = rotation * rotate_z(mat4f(1.0f), deg2rad(value));
                        break;
                    case Channel::Zrotation:
                        rotation = rotation * rotate_x(mat4f(1.0f), deg2rad(value));
                        break;
                    default:
                        break;
                }
                pos++;
            }
            auto [_t, _r, _s] = decompose(rotation);
            anima_builder.AppenTRSToChannel(
                joint,
                Animation::TRS{
                    .translation = translation,
                    .rotation    = euler_to_quaternion(_r),
                    .scaling     = vec3f(1.0f),
                });
        }
    }

    if (pos < tokens.size()) {
        logger->warn("there are some tokens did not handle after parsing: token is {}", tokens[pos]);
    }

    return {root, anima_builder.Finish()};
}

std::vector<std::string> BvhParser::Tokenizer(std::stringstream& ss) {
    std::vector<std::string> result;
    std::string              token;

    while (ss >> token) {
        if (token == "End") {
            ss >> token;
            if (token == "Site") {
                result.push_back("End Site");
            }
        } else if (token == "Frames:") {
            result.push_back("Frames");
        } else if (token == "Frame") {
            ss >> token;
            if (token == "Time:")
                result.push_back("Frame Time");
        } else {
            result.push_back(token);
        }

        token = result.back();
    }
    return result;
}

}  // namespace hitagi::asset