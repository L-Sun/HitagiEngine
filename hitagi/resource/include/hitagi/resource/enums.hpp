#pragma once

#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <cstdint>
#include <type_traits>
#include <concepts>

namespace hitagi::resource {
enum struct PrimitiveType : std::uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    LineListAdjacency,
    LineStripAdjacency,
    TriangleListAdjacency,
    TriangleStripAdjacency,
};

enum struct VertexAttribute : std::uint8_t {
    Position    = 0,  // vec3f
    Color       = 1,  // vec4f
    Normal      = 2,  // vec4f
    Tangent     = 3,  // vec4f
    Bitangent   = 4,  // vec4f
    UV0         = 5,  // vec2u
    UV1         = 6,  // vec2u
    BlendIndex  = 7,  // uint
    BlendWeight = 8,  // float

    // Custom
    Custom6 = 9,
    Custom5 = 10,
    Custom4 = 11,
    Custom3 = 12,
    Custom2 = 13,
    Custom1 = 14,
    Custom0 = 15,

    Num_Support = 16
};

constexpr auto get_vertex_attribute_size(VertexAttribute attribute) {
    switch (attribute) {
        case VertexAttribute::Position:
            return sizeof(math::vec3f);
        case VertexAttribute::Color:
        case VertexAttribute::Normal:
        case VertexAttribute::Tangent:
        case VertexAttribute::Bitangent:
            return sizeof(math::vec4f);
        case VertexAttribute::UV0:
        case VertexAttribute::UV1:
            return sizeof(math::vec2u);
        case VertexAttribute::BlendIndex:
            return sizeof(std::uint32_t);
        case VertexAttribute::BlendWeight:
            return sizeof(float);
        default:
            return 0ull;
    }
}

namespace detial {
template <VertexAttribute e>
constexpr auto vertex_attr() noexcept {
    if constexpr (e == VertexAttribute::Position)
        return math::vec3f{};
    else if constexpr (e == VertexAttribute::Color)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::Normal)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::Tangent)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::Bitangent)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::UV0)
        return math::vec2u{};
    else if constexpr (e == VertexAttribute::UV1)
        return math::vec2u{};
    else if constexpr (e == VertexAttribute::BlendIndex)
        return std::uint32_t{};
    else
        return float{};
}
}  // namespace detial

template <VertexAttribute e>
using VertexType = std::invoke_result_t<decltype(detial::vertex_attr<e>)>;

enum struct IndexType : std::uint8_t {
    UINT16,
    UINT32,
};

constexpr auto get_index_type_size(IndexType type) {
    return type == IndexType::UINT16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
}

enum struct MaterialType : std::uint8_t {
    Phong,
    BlinnPhong,
    Custom
};

template <typename T>
concept MaterialParametric =
    std::same_as<float, T> ||
    std::same_as<std::int32_t, T> ||
    std::same_as<std::uint32_t, T> ||
    std::same_as<math::vec2i, T> ||
    std::same_as<math::vec2u, T> ||
    std::same_as<math::vec2f, T> ||
    std::same_as<math::vec3i, T> ||
    std::same_as<math::vec3u, T> ||
    std::same_as<math::vec3f, T> ||
    std::same_as<math::vec4i, T> ||
    std::same_as<math::vec4u, T> ||
    std::same_as<math::mat4f, T> ||
    std::same_as<math::vec4f, T>;

}  // namespace hitagi::resource