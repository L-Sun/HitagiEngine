#pragma once
#include "../Scene.hpp"
#include "Buffer.hpp"
#include <filesystem>

namespace Hitagi::Asset {
class SceneParser {
public:
    virtual std::shared_ptr<Scene> Parse(const Core::Buffer& buf, const std::filesystem::path& scene_path) = 0;
    virtual ~SceneParser()                                                                                 = default;
};
}  // namespace Hitagi::Asset
