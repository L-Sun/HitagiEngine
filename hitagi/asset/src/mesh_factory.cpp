#include <hitagi/asset/mesh_factory.hpp>

using namespace hitagi::math;

namespace hitagi::asset {

Mesh MeshFactory::Line(const vec3f& from, const vec3f& to) {
    Mesh mesh{
        .vertices = {std::make_shared<VertexArray>(VertexAttribute::Position, 2, "line")},
        .indices  = std::make_shared<IndexArray>(2, "line"),
    };

    mesh.Modify<VertexAttribute::Position>([&](auto pos) {
        pos[0] = from;
        pos[1] = to;
    });

    mesh.Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });

    mesh.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
    });
    return mesh;
}

Mesh MeshFactory::BoxWireframe(const vec3f& bb_min, const vec3f& bb_max) {
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

    Mesh mesh{
        .vertices = {std::make_shared<VertexArray>(VertexAttribute::Position, vertex_data.size(), "box")},
        .indices  = std::make_shared<IndexArray>(index_data.size(), "box"),
    };

    mesh.Modify<VertexAttribute::Position>([&](auto pos) {
        std::copy(vertex_data.cbegin(), vertex_data.cend(), pos.begin());
    });

    mesh.Modify<IndexType::UINT32>([&](auto array) {
        std::copy(index_data.cbegin(), index_data.cend(), array.begin());
    });

    mesh.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = mesh.indices->index_count,
        .index_offset  = 0,
        .vertex_offset = 0,
    });

    return mesh;
}

}  // namespace hitagi::asset