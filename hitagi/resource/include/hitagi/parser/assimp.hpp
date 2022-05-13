#pragma once
#include <hitagi/parser/scene_parser.hpp>

namespace hitagi::resource {
class AssimpParser : public SceneParser {
public:
    void Parse(Scene& scene, const core::Buffer& buffer, allocator_type alloc = {}) final;
};
}  // namespace hitagi::resource