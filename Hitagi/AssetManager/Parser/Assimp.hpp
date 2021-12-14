#pragma once
#include "SceneParser.hpp"

namespace Hitagi::Asset {
class AssimpParser : public SceneParser {
public:
    Scene Parse(const Core::Buffer& buffer) final;
};
}  // namespace Hitagi::Asset