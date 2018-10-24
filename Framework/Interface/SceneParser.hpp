#pragma once
#include <memory>
#include "Interface.hpp"
#include "SceneNode.hpp"

namespace My {
Interface SceneParser {
public:
    virtual std::unique_ptr<BaseSceneNode> Parse(const std::string& buf) = 0;
};
}  // namespace My
