#pragma once
#include <hitagi/asset/parser/scene_parser.hpp>

namespace hitagi::asset {
class AssimpParser : public SceneParser {
public:
    AssimpParser(std::filesystem::path ext);
    auto Parse(const core::Buffer& buffer, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;

private:
    std::filesystem::path m_Hint;
};
}  // namespace hitagi::asset