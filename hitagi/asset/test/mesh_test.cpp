#include "test_macros.hpp"

import asset;
import math;
import test_utils;

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
    EXPECT_VEC_EQ(vertices.Span<VertexAttribute::Position>()[0], hitagi::math::vec3f(2, 3, 4));
    EXPECT_VEC_EQ(vertices.Span<VertexAttribute::Position>()[16], hitagi::math::vec3f(7, 8, 9));
    EXPECT_VEC_EQ(vertices.Span<VertexAttribute::Color0>()[0], hitagi::math::vec4f(2.0f, 3.0f, 4.0f, 1.0f));
    EXPECT_VEC_EQ(vertices.Span<VertexAttribute::Color0>()[16], hitagi::math::vec4f(7.0f, 8.0f, 9.0f, 1.0f));

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

