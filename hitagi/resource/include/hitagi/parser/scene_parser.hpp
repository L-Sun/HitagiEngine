#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class SceneParser {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    virtual void Parse(Scene& scene, const core::Buffer& buffer, allocator_type alloc = {}) = 0;

    virtual ~SceneParser() = default;
};
}  // namespace hitagi::resource
