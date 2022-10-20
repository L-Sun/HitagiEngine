#pragma once
#include <hitagi/asset/scene.hpp>

namespace hitagi::asset {
class MeshFactory {
public:
    static Mesh Line(const math::vec3f& from, const math::vec3f& to);
    static Mesh BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max);
};
}  // namespace hitagi::asset