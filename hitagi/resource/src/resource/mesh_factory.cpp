#include <hitagi/resource/mesh_factory.hpp>

using namespace hitagi::math;

namespace hitagi::resource {
MeshFactory::MeshFactory(allocator_type alloc)
    : m_Allocator(alloc) {}

Mesh MeshFactory::Line(const vec3f& from, const vec3f& to, const vec4f& color) {
    auto vertices = std::allocate_shared<VertexArray>(m_Allocator, 2);
    auto pos      = vertices->GetVertices<VertexAttribute::Position>();
    pos[0]        = from;
    pos[1]        = to;

    auto _color = vertices->GetVertices<VertexAttribute::Color0>();
    _color[0]   = color;
    _color[1]   = color;

    auto indices     = std::allocate_shared<IndexArray>(m_Allocator, 2, IndexType::UINT32);
    auto indices_arr = indices->GetIndices<IndexType::UINT32>();
    indices_arr[0]   = 0;
    indices_arr[1]   = 1;

    return {vertices, indices, nullptr, PrimitiveType::LineList, m_Allocator};
}

Mesh MeshFactory::BoxWireframe(const vec3f& bb_min, const vec3f& bb_max, const vec4f& color) {
    std::array vertex_data = {
        bb_min,                               // 0
        vec3f(bb_max.x, bb_min.y, bb_min.z),  // 1
        vec3f(bb_min.x, bb_max.y, bb_min.z),  // 2
        vec3f(bb_min.x, bb_min.y, bb_max.z),  // 3
        bb_max,                               // 4
        vec3f(bb_min.x, bb_max.y, bb_max.z),  // 5
        vec3f(bb_max.x, bb_min.y, bb_max.z),  // 6
        vec3f(bb_max.x, bb_max.y, bb_min.z),  // 7
    };

    std::array index_data = {
        0u, 1u, 1u, 6u, 6u, 3u, 3u, 0u,  // front
        2u, 7u, 7u, 4u, 4u, 5u, 5u, 2u,  // back
        0u, 2u, 1u, 7u, 6u, 4u, 3u, 5u   // connect two plane
    };

    auto vertices = std::allocate_shared<VertexArray>(m_Allocator, vertex_data.size());
    auto pos      = vertices->GetVertices<VertexAttribute::Position>();
    std::copy(vertex_data.cbegin(), vertex_data.cend(), pos.begin());

    auto _color = vertices->GetVertices<VertexAttribute::Color0>();
    std::fill(_color.begin(), _color.end(), color);

    auto indices       = std::allocate_shared<IndexArray>(m_Allocator, index_data.size(), IndexType::UINT32);
    auto indices_array = indices->GetIndices<IndexType::UINT32>();
    std::copy(index_data.cbegin(), index_data.cend(), indices_array.begin());

    return {vertices, indices, nullptr, PrimitiveType::LineList, m_Allocator};
}

}  // namespace hitagi::resource