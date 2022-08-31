#pragma once
#include <hitagi/parser/scene_parser.hpp>

namespace hitagi::resource {
class AssimpParser : public SceneParser {
public:
    AssimpParser(std::filesystem::path ext);
    std::shared_ptr<Scene> Parse(const core::Buffer& buffer, const std::filesystem::path& root_path) final;

private:
    std::filesystem::path m_Hint;
};
}  // namespace hitagi::resource