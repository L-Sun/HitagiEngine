#include "BvhParser.hpp"

#include <sstream>

namespace Hitagi::Asset {
const std::unordered_map<std::string, BvhParser::Channel> BvhParser::m_ChannelMap = {
    {"Xposition", Channel::Xposition},
    {"Yposition", Channel::Yposition},
    {"Zposition", Channel::Zposition},
    {"Xrotation", Channel::Xrotation},
    {"Yrotation", Channel::Yrotation},
    {"Zrotation", Channel::Zrotation},
};

std::shared_ptr<BoneNode> BvhParser::ParserSkeleton(const Core::Buffer& buffer) {
    std::stringstream ss;
    ss.write(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());

    auto tokens = Tokenizer(ss);

    std::shared_ptr<BoneNode> root;
    std::shared_ptr<BoneNode> current_node;

    std::vector<std::shared_ptr<BoneNode>> stack;
    for (size_t pos = 0; pos < tokens.size(); pos++) {
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
            float y = std::stof(tokens[++pos]);
            float z = std::stof(tokens[++pos]);
            float x = std::stof(tokens[++pos]);

            current_node->Translate(vec3f{x, y, z});
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
    assert(root == current_node);
    return root;
}

std::vector<std::string> BvhParser::Tokenizer(std::stringstream& ss) {
    std::vector<std::string> result;
    std::string              token;
    size_t                   num_nodes = 0;

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
        if (token == "ROOT" || token == "JOINT" || token == "End Site") {
            num_nodes++;
        }
    }
    return result;
}

}  // namespace Hitagi::Asset