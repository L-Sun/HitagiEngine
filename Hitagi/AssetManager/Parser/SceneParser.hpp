#pragma once
#include "../Scene.hpp"
#include "Buffer.hpp"

namespace Hitagi::Asset {
class SceneParser {
public:
    virtual Scene Parse(const Core::Buffer& buf) = 0;
    virtual ~SceneParser()                       = default;
};
}  // namespace Hitagi::Asset
