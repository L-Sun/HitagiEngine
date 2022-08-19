#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class SceneParser {
public:
    virtual std::optional<Scene> Parse(const core::Buffer& buffer) = 0;

    virtual ~SceneParser() = default;
};
}  // namespace hitagi::resource
