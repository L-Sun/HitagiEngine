#pragma once
#include "SceneParser.hpp"

namespace My {
class FbxParser : public SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const Buffer& buf);
};
}  // namespace My