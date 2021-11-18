#pragma once
#include "SceneParser.hpp"

namespace Hitagi::Asset {
class AssimpParser : public SceneParser {
public:
    std::shared_ptr<Scene> Parse(const Core::Buffer& buf, const std::filesystem::path& scene_path) final;
};
}  // namespace Hitagi::Asset