#pragma once
#include <hitagi/resource/mesh.hpp>

namespace hitagi::resource {
class MeshFactory {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    MeshFactory(allocator_type alloc = {});

    Mesh Line(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color);
    Mesh BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max, const math::vec4f& color);

private:
    allocator_type m_Allocator;
};
}  // namespace hitagi::resource