#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class SceneParser {
public:
    virtual void Parse(const core::Buffer& buffer, Scene& scene) = 0;

    virtual ~SceneParser() = default;
};
}  // namespace hitagi::resource
