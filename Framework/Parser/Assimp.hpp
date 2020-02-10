#pragma once
#include "SceneParser.hpp"

namespace My {
class AssimpParser : public SceneParser {
public:
    std::unique_ptr<Scene> Parse(const Buffer& buf) final;
};
}  // namespace My