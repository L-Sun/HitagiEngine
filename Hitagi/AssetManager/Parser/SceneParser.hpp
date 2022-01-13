#pragma once
#include "../Scene.hpp"
#include "Buffer.hpp"

namespace Hitagi::Asset {
class SceneParser {
public:
    virtual std::shared_ptr<Scene> Parse(const Core::Buffer& buffer) = 0;
    virtual ~SceneParser()                                           = default;
};
}  // namespace Hitagi::Asset
