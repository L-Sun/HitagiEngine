#pragma once
#include "Scene.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
class SceneParser {
public:
    virtual std::shared_ptr<Scene> Parse(const Core::Buffer& buf) = 0;
    virtual ~SceneParser() {}
};
}  // namespace Hitagi::Resource
