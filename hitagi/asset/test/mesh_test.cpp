#include <hitagi/resource/mesh.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/test.hpp>

#include <string>

using namespace hitagi::testing;
using namespace hitagi::resource;

TEST(MeshTest, CreateValidVertex) {
    auto vb = VertexArray(VertexAttribute::Position, 256);
    EXPECT_FALSE(vb.Empty());
    EXPECT_EQ(vb.vertex_count, 256);
}

TEST(MeshTest, CreateValidIndex) {
    Mesh mesh{
        .indices = std::make_shared<IndexArray>(20, "CreateValidIndex", IndexType::UINT32),
    };

    EXPECT_EQ(mesh.indices->index_count, 20);
    EXPECT_EQ(mesh.indices->type, IndexType::UINT32);
    EXPECT_EQ(mesh.Span<IndexType::UINT32>().size(), 20);
    EXPECT_EQ(mesh.indices->cpu_buffer.GetDataSize(), 20 * sizeof(std::uint32_t));
}

TEST(MeshTest, Resize) {
    Mesh mesh{
        .vertices = {std::make_shared<VertexArray>(VertexAttribute::Position, 256)},
        .indices  = std::make_shared<IndexArray>(256),
    };

    mesh.vertices[VertexAttribute::Position]->Resize(128);
    EXPECT_EQ(mesh.vertices[VertexAttribute::Position]->vertex_count, 128);
    auto pos = mesh.Span<VertexAttribute::Position>();
    EXPECT_EQ(pos.size(), 128);

    mesh.indices->Resize(128);
    EXPECT_EQ(mesh.indices->index_count, 128);
    EXPECT_EQ(mesh.indices->cpu_buffer.GetDataSize(), 128 * sizeof(std::uint32_t));
}

TEST(MeshTest, Modify) {
    Mesh mesh{
        .indices = std::make_shared<IndexArray>(32),
    };
    mesh.vertices[VertexAttribute::Position] = {std::make_shared<VertexArray>(VertexAttribute::Position, 32)},
    mesh.vertices[VertexAttribute::Color0]   = {std::make_shared<VertexArray>(VertexAttribute::Color0, 32)},

    mesh.Modify<VertexAttribute::Position, VertexAttribute::Color0>([](auto positions, auto colors) {
        positions[0]  = {2, 3, 4};
        positions[16] = {7, 8, 9};
        colors[0]     = {2.0f, 3.0f, 4.0f, 1.0f};
        colors[16]    = {7.0f, 8.0f, 9.0f, 1.0f};
    });
    vector_eq(mesh.Span<VertexAttribute::Position>()[0], hitagi::math::vec3f{2, 3, 4});
    vector_eq(mesh.Span<VertexAttribute::Position>()[16], hitagi::math::vec3f{7, 8, 9});
    vector_eq(mesh.Span<VertexAttribute::Color0>()[0], hitagi::math::vec4f{2.0f, 3.0f, 4.0f, 1.0f});
    vector_eq(mesh.Span<VertexAttribute::Color0>()[16], hitagi::math::vec4f{7.0f, 8.0f, 9.0f, 1.0f});

    mesh.Modify<IndexType::UINT32>([](auto array) {
        array[0]  = 1234;
        array[24] = 5678;
    });
    EXPECT_EQ(mesh.Span<IndexType::UINT32>()[0], 1234);
    EXPECT_EQ(mesh.Span<IndexType::UINT32>()[24], 5678);
    // EXPECT_TRUE(ib.buffer_range_dirty.count(0));
    // EXPECT_TRUE(ib.buffer_range_dirty.count(24));
}

TEST(MeshTest, Merge) {
    Mesh mesh1{
        .vertices = {std::make_shared<VertexArray>(VertexAttribute::Position, 2)},
        .indices  = std::make_shared<IndexArray>(2),
    };
    mesh1.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
    });
    Mesh mesh2{
        .vertices = {std::make_shared<VertexArray>(VertexAttribute::Position, 2)},
        .indices  = std::make_shared<IndexArray>(2),
    };
    mesh2.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
    });

    std::size_t i = 0;
    mesh1.Modify<VertexAttribute::Position>([&](auto positions) {
        for (auto& pos : positions) {
            pos.x = i++;
            pos.y = i++;
            pos.z = i++;
        }
    });
    mesh2.Modify<VertexAttribute::Position>([&](auto positions) {
        for (auto& pos : positions) {
            pos.x = i++;
            pos.y = i++;
            pos.z = i++;
        }
    });
    mesh1.Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });
    mesh2.Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });

    auto mesh = merge_meshes({mesh1, mesh2});
    EXPECT_TRUE(mesh.vertices[VertexAttribute::Position] != nullptr);
    EXPECT_TRUE(mesh.indices != nullptr);
    EXPECT_EQ(mesh.vertices[VertexAttribute::Position]->vertex_count, mesh1.vertices[VertexAttribute::Position]->vertex_count + mesh2.vertices[VertexAttribute::Position]->vertex_count);
    EXPECT_EQ(mesh.indices->index_count, mesh1.indices->index_count + mesh2.indices->index_count);

    i = 0;
    for (auto pos : mesh.Span<VertexAttribute::Position>()) {
        EXPECT_EQ(pos.x, i++);
        EXPECT_EQ(pos.y, i++);
        EXPECT_EQ(pos.z, i++);
    }
    i = 0;
    for (auto index : mesh.Span<IndexType::UINT32>()) {
        EXPECT_EQ(index, i);
        i = !i;
    }
    EXPECT_EQ(mesh.sub_meshes[0].index_count, 2);
    EXPECT_EQ(mesh.sub_meshes[0].vertex_offset, 0);
    EXPECT_EQ(mesh.sub_meshes[0].index_offset, 0);
    EXPECT_EQ(mesh.sub_meshes[1].index_count, 2);
    EXPECT_EQ(mesh.sub_meshes[1].vertex_offset, 2);
    EXPECT_EQ(mesh.sub_meshes[1].index_offset, 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}