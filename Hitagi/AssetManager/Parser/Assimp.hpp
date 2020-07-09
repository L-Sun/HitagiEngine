#pragma once
#include "SceneParser.hpp"

namespace Hitagi::Asset {
class AssimpParser : public SceneParser {
public:
    Scene Parse(const Core::Buffer& buf,const std::filesystem::path& scenePath) final;
};
}  // namespace Hitagi::Asset