#pragma once
#include "Scene.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
class SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const Core::Buffer& buf) = 0;
};
}  // namespace Hitagi::Resource
