#include <hitagi/asset/mesh.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::testing;
using namespace hitagi::asset;

TEST(MeshTest, CreateValidVertex) {
    VertexArray vb(256);
    EXPECT_TRUE(vb.Empty());
    // Enable position
    vb.Modify<VertexAttribute::Position>([](auto pos) {});
    EXPECT_FALSE(vb.Empty());
    EXPECT_EQ(vb.Size(), 256);
}

TEST(MeshTest, CreateValidIndex) {
    IndexArray indices(20, IndexType::UINT16);

    EXPECT_EQ(indices.Size(), 20);
    EXPECT_EQ(indices.Type(), IndexType::UINT16);
    EXPECT_EQ(indices.Span<IndexType::UINT16>().size(), 20);
    EXPECT_EQ(indices.Span<IndexType::UINT32>().size(), 0);
    EXPECT_EQ(indices.GetIndexData().cpu_buffer.GetDataSize(), 20 * sizeof(std::uint16_t));
}

TEST(MeshTest, Resize) {
    VertexArray vertices(256);
    vertices.Modify<VertexAttribute::Position>([](auto pos) {});
    vertices.Resize(128);
    EXPECT_EQ(vertices.Size(), 128);
    EXPECT_EQ(vertices.Span<VertexAttribute::Position>().size(), 128);

    IndexArray indices(256, IndexType::UINT16);
    indices.Resize(128);
    EXPECT_EQ(indices.Size(), 128);
    EXPECT_EQ(indices.GetIndexData().cpu_buffer.GetDataSize(), 128 * sizeof(std::uint16_t));
}

TEST(MeshTest, Modify) {
    VertexArray vertices(32);

    vertices.Modify<VertexAttribute::Position>([](auto positions) {
        positions[0]  = {2, 3, 4};
        positions[16] = {7, 8, 9};
    });
    vertices.Modify<VertexAttribute::Color0>([](auto colors) {
        colors[0]  = {2.0f, 3.0f, 4.0f, 1.0f};
        colors[16] = {7.0f, 8.0f, 9.0f, 1.0f};
    });
    vector_eq(vertices.Span<VertexAttribute::Position>()[0], hitagi::math::vec3f{2, 3, 4});
    vector_eq(vertices.Span<VertexAttribute::Position>()[16], hitagi::math::vec3f{7, 8, 9});
    vector_eq(vertices.Span<VertexAttribute::Color0>()[0], hitagi::math::vec4f{2.0f, 3.0f, 4.0f, 1.0f});
    vector_eq(vertices.Span<VertexAttribute::Color0>()[16], hitagi::math::vec4f{7.0f, 8.0f, 9.0f, 1.0f});

    IndexArray indices(32, IndexType::UINT16);
    indices.Modify<IndexType::UINT16>([](auto array) {
        array[0]  = 1234;
        array[24] = 5678;
    });
    indices.Modify<IndexType::UINT32>([](auto array) {
        EXPECT_TRUE(array.empty());
    });
    EXPECT_EQ(indices.Span<IndexType::UINT16>()[0], 1234);
    EXPECT_EQ(indices.Span<IndexType::UINT16>()[24], 5678);
}

TEST(MeshTest, Merge) {
    Mesh mesh1(std::make_shared<VertexArray>(2), std::make_shared<IndexArray>(2, IndexType::UINT16));
    mesh1.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
    });
    Mesh mesh2(std::make_shared<VertexArray>(2), std::make_shared<IndexArray>(2, IndexType::UINT16));
    mesh2.sub_meshes.emplace_back(Mesh::SubMesh{
        .index_count   = 2,
        .index_offset  = 0,
        .vertex_offset = 0,
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
    mesh1.indices->Modify<IndexType::UINT16>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });
    mesh2.indices->Modify<IndexType::UINT16>([](auto array) {
        array[0] = 0;
        array[1] = 1;
    });

    auto mesh = mesh1 + mesh2;
    EXPECT_TRUE(mesh.vertices != nullptr);
    EXPECT_EQ(mesh.vertices->Size(), mesh1.vertices->Size() + mesh2.vertices->Size());
    EXPECT_FALSE(mesh.vertices->Span<VertexAttribute::Position>().empty());

    EXPECT_TRUE(mesh.indices != nullptr);
    EXPECT_EQ(mesh.indices->Size(), mesh1.indices->Size() + mesh2.indices->Size());

    i = 0;
    for (auto pos : mesh.vertices->Span<VertexAttribute::Position>()) {
        EXPECT_EQ(pos.x, i++);
        EXPECT_EQ(pos.y, i++);
        EXPECT_EQ(pos.z, i++);
    }
    i = 0;
    for (auto index : mesh.indices->Span<IndexType::UINT16>()) {
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