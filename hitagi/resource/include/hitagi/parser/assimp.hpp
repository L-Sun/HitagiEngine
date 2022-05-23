#pragma once
#include <hitagi/parser/scene_parser.hpp>

namespace hitagi::resource {
class AssimpParser : public SceneParser {
public:
    void Parse(Scene& scene, std::pmr::vector<std::shared_ptr<Material>>& materials, const core::Buffer& buffer) final;
};
}  // namespace hitagi::resource