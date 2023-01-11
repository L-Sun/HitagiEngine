#include <hitagi/asset/mesh_factory.hpp>

using namespace hitagi::math;

namespace hitagi::asset {

auto MeshFactory::Line(const vec3f& from, const vec3f& to) -> std::shared_ptr<Mesh> {
    auto mesh = std::make_shared<Mesh>(
        std::make_shared<VertexArray>(2, "line"),
        std::make_shared<IndexArray>(2, IndexType::UINT32, "line"),
        "line");

    mesh->vertices->Modify<VertexAttribute::Position>([&](auto pos) {
        pos[0] = from;
        pos[1] = to;
    });

    mesh->indices->Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });

    mesh->sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
    });
    return mesh;
}

auto MeshFactory::BoxWireframe(const vec3f& bb_min, const vec3f& bb_max) -> std::shared_ptr<Mesh> {
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
    auto mesh = std::make_shared<Mesh>(
        std::make_shared<VertexArray>(vertex_data.size(), "box"),
        std::make_shared<IndexArray>(index_data.size(), IndexType::UINT32, "box"),
        "box");

    mesh->vertices->Modify<VertexAttribute::Position>([&](auto pos) {
        std::copy(vertex_data.cbegin(), vertex_data.cend(), pos.begin());
    });

    mesh->indices->Modify<IndexType::UINT32>([&](auto array) {
        std::copy(index_data.cbegin(), index_data.cend(), array.begin());
    });

    mesh->sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = index_data.size(),
        .index_offset  = 0,
        .vertex_offset = 0,
    });

    return mesh;
}

auto MeshFactory::Cube() -> std::shared_ptr<Mesh> {
    //    7 -------- 6
    //   /|         /|    z  y
    //  3 +------- 2 |    |/
    //  | |        | |    +-- x
    //  | 4 -------+ 5
    //  |/         |/
    //  0 -------- 1
    constexpr std::array cube_positions = {
        vec3f{-0.5, -0.5, -0.5},
        vec3f{+0.5, -0.5, -0.5},
        vec3f{+0.5, -0.5, +0.5},
        vec3f{-0.5, -0.5, +0.5},
        vec3f{-0.5, +0.5, -0.5},
        vec3f{+0.5, +0.5, -0.5},
        vec3f{+0.5, +0.5, +0.5},
        vec3f{-0.5, +0.5, +0.5},
    };
    constexpr std::array<std::uint16_t, 36> cube_indices = {
        0, 1, 2, 2, 3, 0,  // front
        1, 5, 6, 6, 2, 1,  // right
        5, 4, 7, 7, 6, 5,  // back
        0, 3, 7, 7, 4, 0,  // left
        0, 4, 5, 5, 1, 0,  // back
        3, 2, 6, 6, 7, 3,  // top
    };

    auto vertices = std::make_shared<VertexArray>(8, "cube");
    vertices->Modify<VertexAttribute::Position>([&](std::span<vec3f> pos) {
        std::memcpy(pos.data(), cube_positions.data(), sizeof(vec3f) * cube_positions.size());
    });

    auto indices = std::make_shared<IndexArray>(36);
    indices->Modify<IndexType::UINT16>([&](std::span<std::uint16_t> data) {
        std::memcpy(data.data(), cube_indices.data(), sizeof(std::uint16_t) * cube_indices.size());
    });

    auto mesh = std::make_shared<Mesh>(vertices, indices, "Cube");

    mesh->sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = cube_indices.size(),
        .index_offset  = 0,
        .vertex_offset = 0,
    });

    return mesh;
}

}  // namespace hitagi::asset