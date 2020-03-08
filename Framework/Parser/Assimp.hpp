#pragma once
#include "SceneParser.hpp"

namespace Hitagi {
class AssimpParser : public SceneParser {
public:
    std::unique_ptr<Scene> Parse(const Buffer& buf) final;
};
}  // namespace Hitagi