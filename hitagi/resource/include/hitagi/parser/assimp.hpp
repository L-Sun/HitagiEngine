#pragma once
#include "scene_parser.hpp"

namespace hitagi::asset {
class AssimpParser : public SceneParser {
public:
    std::shared_ptr<Scene> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::asset