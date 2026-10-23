#include "test_macros.hpp"
import std;
import interop.gtest;
import asset;
import core;
import math;

using namespace hitagi;
using namespace hitagi::asset;

static_assert(std::same_as<decltype(std::declval<VertexArray&>().Span<VertexAttribute::Position>()), std::span<const math::vec3f>>);
static_assert(std::same_as<decltype(std::declval<IndexArray&>().Span<IndexType::UINT16>()), std::span<const std::uint16_t>>);
static_assert(!std::is_move_assignable_v<VertexArray> && !std::is_move_assignable_v<IndexArray> && !std::is_move_assignable_v<Mesh>);
static_assert(std::is_const_v<decltype(Mesh::sub_meshes)> && std::is_const_v<decltype(Mesh::vertices)> && std::is_const_v<decltype(Mesh::indices)>);

TEST(MeshTest, ConstructionOwnsDataAndExposesOnlyReadViews) {
    const std::array  positions{math::vec3f(2, 3, 4), math::vec3f(7, 8, 9)};
    VertexArray::Data data;
    data[VertexAttribute::Position] = core::Buffer(std::as_bytes(std::span(positions)));
    VertexArray vertices(2, data);
    data[VertexAttribute::Position].Span<math::vec3f>()[0] = math::vec3f(0);
    EXPECT_EQ(vertices.Span<VertexAttribute::Position>()[0], positions[0]);
    EXPECT_EQ(vertices.Span<VertexAttribute::Position>()[1], positions[1]);
    EXPECT_TRUE(vertices.Span<VertexAttribute::Normal>().empty());
    EXPECT_FALSE(vertices.Empty());
    const std::array<std::uint16_t, 3> input{0, 1, 0};
    core::Buffer                       bytes(std::as_bytes(std::span(input)));
    IndexArray                         indices(IndexType::UINT16, bytes);
    bytes.Span<std::uint16_t>()[0] = 1;
    EXPECT_EQ(indices.Size(), 3);
    EXPECT_EQ(indices.Span<IndexType::UINT16>()[0], 0);
    EXPECT_TRUE(indices.Span<IndexType::UINT32>().empty());
}

TEST(MeshTest, RejectsMalformedAttributeAndIndexStorage) {
    VertexArray::Data data;
    data[VertexAttribute::Position] = core::Buffer(1);
    EXPECT_THROW(VertexArray(1, data), std::invalid_argument);
    data[VertexAttribute::Position] = core::Buffer(sizeof(math::vec3f));
    EXPECT_THROW(VertexArray(2, data), std::invalid_argument);
    EXPECT_THROW(VertexArray(std::numeric_limits<std::size_t>::max(), data), std::invalid_argument);
    EXPECT_THROW(IndexArray(IndexType::UINT32, core::Buffer(3)), std::invalid_argument);
    EXPECT_THROW(IndexArray(static_cast<IndexType>(255), core::Buffer{}), std::invalid_argument);
}

TEST(MeshTest, ValidatesSubmeshRangesAndReferencedVertices) {
    const std::array  positions{math::vec3f(0), math::vec3f(1), math::vec3f(2)};
    VertexArray::Data data;
    data[VertexAttribute::Position]             = core::Buffer(std::as_bytes(std::span(positions)));
    auto                               vertices = std::make_shared<VertexArray>(3, std::move(data));
    const std::array<std::uint32_t, 3> values{0, 1, 2};
    auto                               indices = std::make_shared<IndexArray>(IndexType::UINT32, core::Buffer(std::as_bytes(std::span(values))));
    EXPECT_NO_THROW((Mesh(vertices, indices, {{.index_count = 3}})));
    EXPECT_THROW((Mesh(vertices, indices, {{.index_count = 3, .index_offset = 1}})), std::out_of_range);
    EXPECT_THROW((Mesh(vertices, indices, {{.index_count = 3, .index_offset = std::numeric_limits<std::size_t>::max()}})), std::out_of_range);
    EXPECT_THROW((Mesh(vertices, indices, {{.index_count = 3, .vertex_offset = 1}})), std::out_of_range);
    EXPECT_THROW((Mesh(vertices, indices, {{.index_count = 0}})), std::out_of_range);
    EXPECT_THROW((Mesh({}, indices, {{.index_count = 3}})), std::invalid_argument);
}
