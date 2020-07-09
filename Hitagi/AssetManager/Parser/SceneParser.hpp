#pragma once
#include "../Scene.hpp"
#include "Buffer.hpp"
#include <filesystem>

namespace Hitagi::Asset {
class SceneParser {
public:
    virtual Scene Parse(const Core::Buffer& buf, const std::filesystem::path& scenePath) = 0;
    virtual ~SceneParser()                                                               = default;
};
}  // namespace Hitagi::Asset
