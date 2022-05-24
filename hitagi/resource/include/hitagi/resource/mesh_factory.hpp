#pragma once
#include <hitagi/resource/mesh.hpp>

namespace hitagi::resource {
class MeshFactory {
public:
    static Mesh Line(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color);
    static Mesh BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max, const math::vec4f& color);

private:
    static std::shared_ptr<MaterialInstance> LineMaterial();
    static std::shared_ptr<MaterialInstance> WireframeMaterial();
};
}  // namespace hitagi::resource