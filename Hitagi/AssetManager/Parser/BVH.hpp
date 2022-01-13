#pragma once
#include "MoCapParser.hpp"

#include <unordered_map>

namespace Hitagi::Asset {
class BvhParser : public MoCapParser {
public:
    virtual std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> Parse(const Core::Buffer& buffer);

private:
    enum struct Channel {
        Xposition,
        Yposition,
        Zposition,
        Xrotation,
        Yrotation,
        Zrotation,
    };
    static const std::unordered_map<std::string, Channel> m_ChannelMap;

    static std::vector<std::string> Tokenizer(std::stringstream& ss);
};
}  // namespace Hitagi::Asset
