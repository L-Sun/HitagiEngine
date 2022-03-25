#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::asset {
class SceneParser {
public:
    virtual std::shared_ptr<Scene> Parse(const core::Buffer& buffer) = 0;
    virtual ~SceneParser()                                           = default;
};
}  // namespace hitagi::asset
