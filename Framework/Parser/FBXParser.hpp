#pragma once
#include "SceneParser.hpp"

namespace My {
class SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const std::string& buf){
        
    };
};
}  // namespace My
