#pragma once
#include "SceneParser.hpp"

namespace Hitagi::Resource {
class AssimpParser : public SceneParser {
public:
    Scene Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource