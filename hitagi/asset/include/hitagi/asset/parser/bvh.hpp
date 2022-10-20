#pragma once
#include "mocap_parser.hpp"

#include <unordered_map>

namespace hitagi::asset {
class BvhParser : public MoCapParser {
public:
    virtual std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> Parse(const core::Buffer& buffer);

private:
    enum struct Channel {
        Xposition,
        Yposition,
        Zposition,
        Xrotation,
        Yrotation,
        Zrotation,
    };
    static const std::pmr::unordered_map<std::pmr::string, Channel> m_ChannelMap;

    static std::pmr::vector<std::pmr::string> Tokenizer(std::stringstream& ss);
};
}  // namespace hitagi::asset
