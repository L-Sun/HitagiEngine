#pragma once
#include "../SceneParser.hpp"

namespace Hitagi::Resource {
class AssimpParser : public SceneParser {
public:
    std::shared_ptr<Scene> Parse(const Core::Buffer& buf) final;
};
}  // namespace Hitagi::Resource