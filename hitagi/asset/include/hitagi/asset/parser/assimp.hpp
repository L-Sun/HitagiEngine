#pragma once
#include <hitagi/asset/parser/scene_parser.hpp>

namespace hitagi::asset {
class AssimpParser : public SceneParser {
public:
    using SceneParser::SceneParser;

    AssimpParser(std::filesystem::path ext = "", std::shared_ptr<spdlog::logger> logger = nullptr) : SceneParser(std::move(logger)), m_Hint(std::move(ext)) {}
    auto Parse(const core::Buffer& buffer, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;

private:
    std::filesystem::path m_Hint;
};
}  // namespace hitagi::asset