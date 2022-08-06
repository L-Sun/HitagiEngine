#pragma once
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class MeshFactory {
public:
    static Mesh Line(const math::vec3f& from, const math::vec3f& to);
    static Mesh BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max);
};
}  // namespace hitagi::resource