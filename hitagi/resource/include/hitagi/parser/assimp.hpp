#pragma once
#include <hitagi/parser/scene_parser.hpp>

namespace hitagi::resource {
class AssimpParser : public SceneParser {
public:
    std::optional<Scene> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::resource