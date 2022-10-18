#pragma once
#include <hitagi/asset/parser/scene_parser.hpp>

namespace hitagi::asset {
class AssimpParser : public SceneParser {
public:
    using SceneParser::SceneParser;

    auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;
};
}  // namespace hitagi::asset