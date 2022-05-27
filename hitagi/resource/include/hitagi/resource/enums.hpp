#pragma once

#include <hitagi/math/vector.hpp>
#include <hitagi/math/matrix.hpp>

#include <magic_enum.hpp>

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
    Unkown,
};

enum struct VertexAttribute : std::uint8_t {
    Position    = 0,   // vec3f
    Normal      = 1,   // vec3f
    Tangent     = 2,   // vec3f
    Bitangent   = 3,   // vec3f
    Color0      = 4,   // vec4f
    Color1      = 5,   // vec4f
    Color2      = 6,   // vec4f
    Color3      = 7,   // vec4f
    UV0         = 8,   // vec2f
    UV1         = 9,   // vec2f
    UV2         = 10,  // vec2f
    UV3         = 11,  // vec2f
    BlendIndex  = 12,  // uint
    BlendWeight = 13,  // float

    // Custom the default value type is float
    Custom1 = 14,
    Custom0 = 15,
};

namespace detial {
template <VertexAttribute e>
constexpr auto vertex_attr() noexcept {
    /**/ if constexpr (e == VertexAttribute::Position ||
                       e == VertexAttribute::Normal ||
                       e == VertexAttribute::Tangent ||
                       e == VertexAttribute::Bitangent)
        return math::vec3f{};
    else if constexpr (e == VertexAttribute::Color0 ||
                       e == VertexAttribute::Color1 ||
                       e == VertexAttribute::Color2 ||
                       e == VertexAttribute::Color3)
        return math::vec4f{};
    else if constexpr (e == VertexAttribute::UV0 ||
                       e == VertexAttribute::UV1 ||
                       e == VertexAttribute::UV2 ||
                       e == VertexAttribute::UV3)
        return math::vec2f{};
    else if constexpr (e == VertexAttribute::BlendIndex)
        return std::uint32_t{};
    else if constexpr (e == VertexAttribute::BlendWeight)
        return float{};
    else
        return float{};
}
}  // namespace detial

template <VertexAttribute e>
using VertexType = std::invoke_result_t<decltype(detial::vertex_attr<e>)>;

constexpr std::size_t get_vertex_attribute_size(VertexAttribute attribute) {
    return magic_enum::enum_switch(
        [](auto e) {
            return sizeof(VertexType<e>);
        },
        attribute, 4);
}

enum struct IndexType : std::uint8_t {
    UINT16,
    UINT32,
};

template <IndexType t>
using IndexDataType = std::conditional_t<t == IndexType::UINT16, std::uint16_t, std::uint32_t>;

constexpr auto get_index_type_size(IndexType type) {
    return type == IndexType::UINT16 ? sizeof(std::uint16_t) : sizeof(std::uint32_t);
}

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