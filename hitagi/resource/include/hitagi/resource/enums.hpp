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

enum struct TextureAddressMode {
    Wrap,
    Mirror,
    Clamp,
    Border,
    MirrorOnce
};

enum struct ComparisonFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum struct Filter : std::uint32_t {
    Min_Mag_Mip_Point                          = 0,
    Min_Mag_Point_Mip_Linear                   = 0x1,
    Min_Point_Mag_Linear_Mip_Point             = 0x4,
    Min_Point_Mag_Mip_Linear                   = 0x5,
    Min_Linear_Mag_Mip_Point                   = 0x10,
    Min_Linear_Mag_Point_Mip_Linear            = 0x11,
    Min_Mag_Linear_Mip_Point                   = 0x14,
    Min_Mag_Mip_Linear                         = 0x15,
    Anisotropic                                = 0x55,
    Comparison_Min_Mag_Mip_Point               = 0x80,
    Comparison_Min_Mag_Point_Mip_Linear        = 0x81,
    Comparison_Min_Point_Mag_Linear_Mip_Point  = 0x84,
    Comparison_Min_Point_Mag_Mip_Linear        = 0x85,
    Comparison_Min_Linear_Mag_Mip_Point        = 0x90,
    Comparison_Min_Linear_Mag_Point_Mip_Linear = 0x91,
    Comparison_Min_Mag_Linear_Mip_Point        = 0x94,
    Comparison_Min_Mag_Mip_Linear              = 0x95,
    Comparison_Anisotropic                     = 0xd5,
    Minimum_Min_Mag_Mip_Point                  = 0x100,
    Minimum_Min_Mag_Point_Mip_Linear           = 0x101,
    Minimum_Min_Point_Mag_Linear_Mip_Point     = 0x104,
    Minimum_Min_Point_Mag_Mip_Linear           = 0x105,
    Minimum_Min_Linear_Mag_Mip_Point           = 0x110,
    Minimum_Min_Linear_Mag_Point_Mip_Linear    = 0x111,
    Minimum_Min_Mag_Linear_Mip_Point           = 0x114,
    Minimum_Min_Mag_Mip_Linear                 = 0x115,
    Minimum_Anisotropic                        = 0x155,
    Maximum_Min_Mag_Mip_Point                  = 0x180,
    Maximum_Min_Mag_Point_Mip_Linear           = 0x181,
    Maximum_Min_Point_Mag_Linear_Mip_Point     = 0x184,
    Maximum_Min_Point_Mag_Mip_Linear           = 0x185,
    Maximum_Min_Linear_Mag_Mip_Point           = 0x190,
    Maximum_Min_Linear_Mag_Point_Mip_Linear    = 0x191,
    Maximum_Min_Mag_Linear_Mip_Point           = 0x194,
    Maximum_Min_Mag_Mip_Linear                 = 0x195,
    Maximum_Anisotropic                        = 0x1d5
};

struct SamplerDesc {
    Filter             filter         = Filter::Anisotropic;
    TextureAddressMode address_u      = TextureAddressMode::Wrap;
    TextureAddressMode address_v      = TextureAddressMode::Wrap;
    TextureAddressMode address_w      = TextureAddressMode::Wrap;
    float              mip_lod_bias   = 0;
    unsigned           max_anisotropy = 16;
    ComparisonFunc     comp_func      = ComparisonFunc::LessEqual;
    math::vec4f        border_color   = math::vec4f(0.0f);
    float              min_lod        = 0.0f;
    float              max_lod        = std::numeric_limits<float>::max();
};

}  // namespace hitagi::resource