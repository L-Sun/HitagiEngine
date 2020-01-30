#pragma once
#include "Scene.hpp"
#include "Buffer.hpp"

namespace My {
class SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const std::string& filePath) = 0;
};
}  // namespace My
