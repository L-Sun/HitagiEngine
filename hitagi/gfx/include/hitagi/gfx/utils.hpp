#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/gfx/sync.hpp>

#include <magic_enum_flags.hpp>

#include <array>

namespace hitagi::gfx {

inline auto split_semantic(std::string_view semantic) -> std::pair<std::string_view, std::uint32_t> {
    using namespace std::string_view_literals;

    constexpr std::array semantic_names = {
        "POSITION"sv,
        "NORMAL"sv,
        "TANGENT"sv,
        "BINORMAL"sv,
        "COLOR"sv,
        "TEXCOORD"sv,
        "BLENDINDICES"sv,
        "BLENDWEIGHT"sv,
        "PSIZE"sv,
    };

    for (const auto& name : semantic_names) {
        if (semantic.starts_with(name)) {
            if (semantic.size() == name.size()) {
                return {name, 0};
            } else {
                return {name, std::stoul(std::string{semantic.substr(name.size())})};
            }
        }
    }

    throw std::invalid_argument(fmt::format("Unkown semantic: {}", semantic));
}

inline constexpr auto get_format_bit_size(Format format) noexcept -> std::size_t {
    switch (format) {
        case Format::R32G32B32A32_TYPELESS:
        case Format::R32G32B32A32_FLOAT:
        case Format::R32G32B32A32_UINT:
        case Format::R32G32B32A32_SINT:
            return 128;
        case Format::R32G32B32_TYPELESS:
        case Format::R32G32B32_FLOAT:
        case Format::R32G32B32_UINT:
        case Format::R32G32B32_SINT:
            return 96;
        case Format::R16G16B16A16_TYPELESS:
        case Format::R16G16B16A16_FLOAT:
        case Format::R16G16B16A16_UNORM:
        case Format::R16G16B16A16_UINT:
        case Format::R16G16B16A16_SNORM:
        case Format::R16G16B16A16_SINT:
        case Format::R32G32_TYPELESS:
        case Format::R32G32_FLOAT:
        case Format::R32G32_UINT:
        case Format::R32G32_SINT:
        case Format::R32G8X24_TYPELESS:
        case Format::D32_FLOAT_S8X24_UINT:
        case Format::R32_FLOAT_X8X24_TYPELESS:
        case Format::X32_TYPELESS_G8X24_UINT:
            return 64;
        case Format::R10G10B10A2_TYPELESS:
        case Format::R10G10B10A2_UNORM:
        case Format::R10G10B10A2_UINT:
        case Format::R11G11B10_FLOAT:
        case Format::R8G8B8A8_TYPELESS:
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_UNORM_SRGB:
        case Format::R8G8B8A8_UINT:
        case Format::R8G8B8A8_SNORM:
        case Format::R8G8B8A8_SINT:
        case Format::R16G16_TYPELESS:
        case Format::R16G16_FLOAT:
        case Format::R16G16_UNORM:
        case Format::R16G16_UINT:
        case Format::R16G16_SNORM:
        case Format::R16G16_SINT:
        case Format::R32_TYPELESS:
        case Format::D32_FLOAT:
        case Format::R32_FLOAT:
        case Format::R32_UINT:
        case Format::R32_SINT:
        case Format::R24G8_TYPELESS:
        case Format::D24_UNORM_S8_UINT:
        case Format::R24_UNORM_X8_TYPELESS:
        case Format::X24_TYPELESS_G8_UINT:
            return 32;
        case Format::R8G8_TYPELESS:
        case Format::R8G8_UNORM:
        case Format::R8G8_UINT:
        case Format::R8G8_SNORM:
        case Format::R8G8_SINT:
        case Format::R16_TYPELESS:
        case Format::R16_FLOAT:
        case Format::D16_UNORM:
        case Format::R16_UNORM:
        case Format::R16_UINT:
        case Format::R16_SNORM:
        case Format::R16_SINT:
            return 16;
        case Format::R8_TYPELESS:
        case Format::R8_UNORM:
        case Format::R8_UINT:
        case Format::R8_SNORM:
        case Format::R8_SINT:
        case Format::A8_UNORM:
            return 8;
        case Format::R1_UNORM:
            return 1;
        default:
            return 0;
    }
}

inline constexpr auto get_format_byte_size(Format format) noexcept -> std::size_t {
    return get_format_bit_size(format) >> 3;
}

inline auto format_as(ResourceType type) noexcept {
    return magic_enum::enum_name(type);
}

inline auto format_as(GPUBufferUsageFlags usages) noexcept {
    return magic_enum::enum_flags_name(usages);
}

inline auto format_as(TextureUsageFlags usages) noexcept {
    return magic_enum::enum_flags_name(usages);
}

inline auto format_as(ShaderType type) noexcept {
    return magic_enum::enum_name(type);
}

inline auto format_as(Format format) noexcept {
    return magic_enum::enum_name(format);
}

inline auto formate_as(PrimitiveTopology topology) noexcept {
    return magic_enum::enum_name(topology);
}

inline auto format_as(FillMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(CullMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(FrontFace mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(ColorMask mask) noexcept {
    return magic_enum::enum_flags_name(mask);
}

inline auto format_as(StencilOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(BlendFactor factor) noexcept {
    return magic_enum::enum_name(factor);
}

inline auto format_as(BlendOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(LogicOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(CompareOp op) noexcept {
    return magic_enum::enum_name(op);
}

inline auto format_as(AddressMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(FilterMode mode) noexcept {
    return magic_enum::enum_name(mode);
}

inline auto format_as(PipelineStage stage) noexcept {
    return magic_enum::enum_flags_name(stage);
}

inline auto format_as(BarrierAccess access) noexcept {
    return magic_enum::enum_flags_name(access);
}

inline auto format_as(TextureLayout layout) noexcept {
    return magic_enum::enum_flags_name(layout);
}

}  // namespace hitagi::gfx