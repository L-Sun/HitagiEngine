#include <hitagi/utils/test.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <string>

using namespace hitagi::testing;
using namespace hitagi::resource;

TEST(MeshTest, CreateValidVertex) {
    auto vb = VertexArray(256);
    EXPECT_EQ(vb.vertex_count, 256);
    auto pos = vb.GetVertices<VertexAttribute::Position>();
    EXPECT_EQ(pos.size(), 256);
    auto normal = vb.GetVertices<VertexAttribute::Normal>();
    EXPECT_EQ(normal.size(), 256);
}

TEST(MeshTest, CreateValidIndex) {
    auto ib = IndexArray(20, IndexType::UINT32);

    EXPECT_EQ(ib.index_count, 20);
    EXPECT_EQ(ib.type, IndexType::UINT32);
    EXPECT_EQ(ib.GetIndices<IndexType::UINT32>().size(), 20);
    EXPECT_EQ(ib.cpu_buffer.GetDataSize(), 20 * sizeof(std::uint32_t));
}

TEST(MeshTest, Resize) {
    auto vb = VertexArray(256);
    vb.Resize(128);
    EXPECT_EQ(vb.vertex_count, 128);
    auto pos = vb.GetVertices<VertexAttribute::Position>();
    EXPECT_EQ(pos.size(), 128);

    auto ib = IndexArray(256, IndexType::UINT32);
    ib.Resize(128);
    EXPECT_EQ(ib.index_count, 128);
    EXPECT_EQ(ib.cpu_buffer.GetDataSize(), 128 * sizeof(std::uint32_t));
}

TEST(MeshTest, Modify) {
    auto vb = VertexArray(32);
    vb.Modify<VertexAttribute::Position>([](auto positions) {
        positions[0]  = {2, 3, 4};
        positions[16] = {7, 8, 9};
    });
    vector_eq(vb.GetVertices<VertexAttribute::Position>()[0], hitagi::math::vec3f{2, 3, 4});
    vector_eq(vb.GetVertices<VertexAttribute::Position>()[16], hitagi::math::vec3f{7, 8, 9});

    vb.Modify<VertexAttribute::Color0>([](auto positions) {
        positions[0]  = {2.0f, 3.0f, 4.0f, 1.0f};
        positions[16] = {7.0f, 8.0f, 9.0f, 1.0f};
    });
    vector_eq(vb.GetVertices<VertexAttribute::Position>()[0], hitagi::math::vec3f{2, 3, 4});
    vector_eq(vb.GetVertices<VertexAttribute::Position>()[16], hitagi::math::vec3f{7, 8, 9});
    vector_eq(vb.GetVertices<VertexAttribute::Color0>()[0], hitagi::math::vec4f{2.0f, 3.0f, 4.0f, 1.0f});
    vector_eq(vb.GetVertices<VertexAttribute::Color0>()[16], hitagi::math::vec4f{7.0f, 8.0f, 9.0f, 1.0f});

    auto ib = IndexArray(32, IndexType::UINT32);

    ib.Modify<IndexType::UINT32>([](auto array) {
        array[0]  = 1234;
        array[24] = 5678;
    });
    EXPECT_EQ(ib.GetIndices<IndexType::UINT32>()[0], 1234);
    EXPECT_EQ(ib.GetIndices<IndexType::UINT32>()[24], 5678);
    // EXPECT_TRUE(ib.buffer_range_dirty.count(0));
    // EXPECT_TRUE(ib.buffer_range_dirty.count(24));
}

TEST(MeshTest, Merge) {
    Mesh mesh1{
        .vertices = std::make_shared<VertexArray>(2),
        .indices  = std::make_shared<IndexArray>(2, IndexType::UINT32),
    };
    mesh1.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .vertex_offset = 0,
        .index_offset  = 0,
    });
    Mesh mesh2{
        .vertices = std::make_shared<VertexArray>(2),
        .indices  = std::make_shared<IndexArray>(2, IndexType::UINT32),
    };
    mesh2.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .vertex_offset = 0,
        .index_offset  = 0,
    });

    std::size_t i = 0;
    mesh1.vertices->Modify<VertexAttribute::Position>([&](auto positions) {
        for (auto& pos : positions) {
            pos.x = i++;
            pos.y = i++;
            pos.z = i++;
        }
    });
    mesh2.vertices->Modify<VertexAttribute::Position>([&](auto positions) {
        for (auto& pos : positions) {
            pos.x = i++;
            pos.y = i++;
            pos.z = i++;
        }
    });
    mesh1.indices->Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });
    mesh2.indices->Modify<IndexType::UINT32>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });

    auto mesh = merge_meshes({mesh1, mesh2});
    EXPECT_TRUE(mesh.vertices != nullptr);
    EXPECT_TRUE(mesh.indices != nullptr);
    EXPECT_EQ(mesh.vertices->vertex_count, mesh1.vertices->vertex_count + mesh2.vertices->vertex_count);
    EXPECT_EQ(mesh.indices->index_count, mesh1.indices->index_count + mesh2.indices->index_count);

    i = 0;
    for (auto pos : mesh.vertices->GetVertices<VertexAttribute::Position>()) {
        EXPECT_EQ(pos.x, i++);
        EXPECT_EQ(pos.y, i++);
        EXPECT_EQ(pos.z, i++);
    }
    i = 0;
    for (auto index : mesh.indices->GetIndices<IndexType::UINT32>()) {
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