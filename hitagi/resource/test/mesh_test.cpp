#include <hitagi/utils/test.hpp>
#include <hitagi/resource/mesh.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <string>

using namespace hitagi::resource;

TEST(MeshTest, CreateValidVertex) {
    auto vb = VertexArray(256);
    EXPECT_EQ(vb.VertexCount(), 256);
    auto pos = vb.GetVertices<VertexAttribute::Position>();
    EXPECT_EQ(pos.size(), 256);
    auto normal = vb.GetVertices<VertexAttribute::Normal>();
    EXPECT_EQ(normal.size(), 256);
}

TEST(MeshTest, CreateInvalidVertex) {
}

TEST(MeshTest, CreateValidIndex) {
    auto ib = IndexArray(20, IndexType::UINT16);

    EXPECT_EQ(ib.IndexCount(), 20);
    EXPECT_EQ(ib.IndexSize(), sizeof(std::uint16_t));
    EXPECT_EQ(ib.GetIndices<IndexType::UINT16>().size(), 20);
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    ::testing::InitGoogleTest(&argc, argv);
    auto p = spdlog::stdout_color_st(std::string{"AssetManager"});
    return RUN_ALL_TESTS();
}