#pragma once
#include "MoCapParser.hpp"

#include <unordered_map>

namespace Hitagi::Asset {
class BvhParser : public MoCapParser {
public:
    virtual std::shared_ptr<BoneNode> ParserSkeleton(const Core::Buffer& buffer) final;
    virtual ~BvhParser() = default;

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
