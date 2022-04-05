#include <hitagi/resource/mesh.hpp>

#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <string>
#include "spdlog/common.h"

using namespace hitagi::resource;

TEST(MeshTest, CreateValidVertex) {
    auto vb = VertexArray::Builder()
                  .VertexCount(16)
                  .BufferCount(3)
                  .AppendAttributeAt(0, VertexAttribute::Position)
                  .AppendAttributeAt(0, VertexAttribute::Position)
                  .AppendAttributeAt(0, VertexAttribute::Color)
                  .AppendAttributeAt(1, VertexAttribute::Tangent)
                  .AppendAttributeAt(1, VertexAttribute::Position)
                  .AppendAttributeAt(2, VertexAttribute::BlendIndex)
                  .Build();
    EXPECT_TRUE(vb != nullptr);
    EXPECT_EQ(vb->VertexCount(), 16);
    EXPECT_EQ(vb->GetStride(0), get_vertex_attribute_size(VertexAttribute::Position) + get_vertex_attribute_size(VertexAttribute::Color));
    EXPECT_EQ(vb->GetStride(1), get_vertex_attribute_size(VertexAttribute::Tangent));
    EXPECT_EQ(vb->GetStride(2), get_vertex_attribute_size(VertexAttribute::BlendIndex));
    EXPECT_EQ(vb->GetBuffer(0).GetDataSize(), 16 * (get_vertex_attribute_size(VertexAttribute::Position) + get_vertex_attribute_size(VertexAttribute::Color)));
    EXPECT_EQ(vb->GetBuffer(1).GetDataSize(), 16 * get_vertex_attribute_size(VertexAttribute::Tangent));
    EXPECT_EQ(vb->GetBuffer(2).GetDataSize(), 16 * get_vertex_attribute_size(VertexAttribute::BlendIndex));
}

TEST(MeshTest, CreateInvalidVertex) {
    auto vb1 = VertexArray::Builder()
                   .VertexCount(0)
                   .BufferCount(2)
                   .Build();
    EXPECT_TRUE(vb1 == nullptr);

    auto vb2 = VertexArray::Builder()
                   .VertexCount(1)
                   .BufferCount(2)
                   .AppendAttributeAt(0, VertexAttribute::Position)
                   .Build();
    EXPECT_TRUE(vb2 == nullptr);

    auto vb3 = VertexArray::Builder()
                   .VertexCount(1)
                   .BufferCount(2)
                   .AppendAttributeAt(0, VertexAttribute::Position)
                   .AppendAttributeAt(1, VertexAttribute::Position)
                   .Build();
    EXPECT_TRUE(vb3 == nullptr);

    auto vb4 = VertexArray::Builder()
                   .VertexCount(1)
                   .BufferCount(2)
                   .AppendAttributeAt(0, VertexAttribute::Position)
                   .AppendAttributeAt(1, VertexAttribute::Color)
                   .AppendAttributeAt(3, VertexAttribute::Tangent)
                   .Build();
    EXPECT_TRUE(vb4 == nullptr);
}

TEST(MeshTest, CreateValidIndex) {
    auto ib = IndexArray::Builder()
                  .Count(20)
                  .Type(IndexType::UINT16)
                  .Build();
    EXPECT_TRUE(ib != nullptr);
    EXPECT_EQ(ib->IndexCount(), 20);
    EXPECT_EQ(ib->IndexSize(), sizeof(std::uint16_t));
    EXPECT_EQ(ib->Buffer().GetDataSize(), 20 * sizeof(std::uint16_t));
}

TEST(MeshTest, CreateInValidIndex) {
    auto ib = IndexArray::Builder()
                  .Count(0)
                  .Type(IndexType::UINT16)
                  .Build();
    EXPECT_TRUE(ib == nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    auto p = spdlog::stdout_color_st(std::string{"AssetManager"});
    p->set_level(spdlog::level::off);
    return RUN_ALL_TESTS();
}