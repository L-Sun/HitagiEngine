#pragma once
#include <fbxsdk.h>
#include "SceneParser.hpp"

namespace My {
class FbxParser : public SceneParser {
public:
    virtual std::unique_ptr<Scene> Parser(const Buffer& buf) {}
};
}  // namespace My