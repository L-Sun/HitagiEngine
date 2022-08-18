#pragma once
#include <hitagi/parser/scene_parser.hpp>

namespace hitagi::resource {
class AssimpParser : public SceneParser {
public:
    void Parse(const core::Buffer& buffer, Scene& scene) final;
};
}  // namespace hitagi::resource