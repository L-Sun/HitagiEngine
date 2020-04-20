#pragma once
#include "SceneParser.hpp"

namespace Hitagi::Resource {
class AssimpParser : public SceneParser {
public:
    AssimpParser() : SceneParser(spdlog::stdout_color_st("AssimpParser")) {}
    std::unique_ptr<Scene> Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource