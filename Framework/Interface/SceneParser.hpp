#pragma once
#include "Scene.hpp"

namespace My {
class SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const std::string& buf) = 0;
};
}  // namespace My
