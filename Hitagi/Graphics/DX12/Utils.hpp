#pragma once
#include "D3Dpch.hpp"
#include "Resource.hpp"

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

// TODO change when integrate Vulkan
namespace Hitagi::Graphics::backend::DX12 {
inline const size_t align(size_t x, size_t a) {
    assert(((a - 1) & a) == 0 && "alignment is not a power of two");
    return (x + a - 1) & ~(a - 1);
}

inline DXGI_FORMAT to_dxgi_format(Graphics::Format format) noexcept { return static_cast<DXGI_FORMAT>(format); }

inline Graphics::Format to_format(DXGI_FORMAT format) noexcept { return static_cast<Graphics::Format>(format); }

inline D3D12_TEXTURE_ADDRESS_MODE to_d3d_texture_address_mode(Graphics::TextureAddressMode mode) {
    switch (mode) {
        case Graphics::TextureAddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case Graphics::TextureAddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case Graphics::TextureAddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case Graphics::TextureAddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case Graphics::TextureAddressMode::MirrorOnce:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert Texture Address Mode ({}) to D3D", magic_enum::enum_name(mode));
                logger->warn("Will return default value!");
            }
        }
    }
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

inline D3D12_COMPARISON_FUNC to_d3d_comp_func(Graphics::ComparisonFunc func) {
    switch (func) {
        case Graphics::ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case Graphics::ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case Graphics::ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case Graphics::ComparisonFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case Graphics::ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case Graphics::ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case Graphics::ComparisonFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case Graphics::ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert ComparisonFunc ({}) to D3D", magic_enum::enum_name(func));
                logger->warn("Will return Never Func!");
            }
        }
    }
    return D3D12_COMPARISON_FUNC_NEVER;
}

// TODO change when integrate Vulkan
inline D3D12_FILTER to_d3d_filter(Graphics::Filter filter) noexcept {
    return static_cast<D3D12_FILTER>(filter);
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE to_dx_topology_type(PrimitiveType type) noexcept {
    switch (type) {
        case PrimitiveType::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveType::LineList:
        case PrimitiveType::LineStrip:
        case PrimitiveType::LineListAdjacency:
        case PrimitiveType::LineStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveType::TriangleList:
        case PrimitiveType::TriangleStrip:
        case PrimitiveType::TriangleListAdjacency:
        case PrimitiveType::TriangleStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

inline D3D12_PRIMITIVE_TOPOLOGY to_dx_topology(PrimitiveType type) noexcept {
    switch (type) {
        case PrimitiveType::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveType::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveType::LineListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case PrimitiveType::LineStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case PrimitiveType::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::TriangleListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveType::TriangleStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    }
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

inline DXGI_FORMAT index_size_to_dxgi_format(size_t size) noexcept {
    if (size == 4) {
        return DXGI_FORMAT_R32_UINT;
    } else if (size == 2) {
        return DXGI_FORMAT_R16_UINT;
    } else if (size == 1) {
        return DXGI_FORMAT_R8_UINT;
    } else {
        return DXGI_FORMAT_UNKNOWN;
    }
}

inline D3D12_SAMPLER_DESC to_d3d_sampler_desc(Graphics::Sampler::Description desc) noexcept {
    D3D12_SAMPLER_DESC result{};
    result.AddressU       = to_d3d_texture_address_mode(desc.address_u);
    result.AddressV       = to_d3d_texture_address_mode(desc.address_v);
    result.AddressW       = to_d3d_texture_address_mode(desc.address_w);
    result.BorderColor[0] = desc.border_color.r;
    result.BorderColor[1] = desc.border_color.g;
    result.BorderColor[2] = desc.border_color.b;
    result.BorderColor[3] = desc.border_color.a;
    result.ComparisonFunc = to_d3d_comp_func(desc.comp_func);
    result.Filter         = to_d3d_filter(desc.filter);
    result.MaxAnisotropy  = desc.max_anisotropy;
    result.MaxLOD         = desc.max_lod;
    result.MinLOD         = desc.min_lod;
    result.MipLODBias     = desc.mip_lod_bias;

    return result;
}

inline D3D12_SHADER_VISIBILITY to_d3d_shader_visibility(Graphics::ShaderVisibility visibility) {
    switch (visibility) {
        case Graphics::ShaderVisibility::All:
            return D3D12_SHADER_VISIBILITY_ALL;
        case Graphics::ShaderVisibility::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case Graphics::ShaderVisibility::Hull:
            return D3D12_SHADER_VISIBILITY_HULL;
        case Graphics::ShaderVisibility::Domain:
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case Graphics::ShaderVisibility::Geometry:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case Graphics::ShaderVisibility::Pixel:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert Visibility ({}) to D3D", magic_enum::enum_name(visibility));
                logger->warn("Will return D3D12_SHADER_VISIBILITY_ALL!");
            }
        }
    }
    return D3D12_SHADER_VISIBILITY_ALL;
}

inline D3D12_BLEND to_d3d_blend(Graphics::Blend blend) {
    switch (blend) {
        case Graphics::Blend::Zero:
            return D3D12_BLEND_ZERO;
        case Graphics::Blend::One:
            return D3D12_BLEND_ONE;
        case Graphics::Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case Graphics::Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case Graphics::Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case Graphics::Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case Graphics::Blend::DestAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case Graphics::Blend::InvDestAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case Graphics::Blend::DestColor:
            return D3D12_BLEND_DEST_COLOR;
        case Graphics::Blend::InvDestColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case Graphics::Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case Graphics::Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case Graphics::Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case Graphics::Blend::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case Graphics::Blend::InvSrc_1_Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case Graphics::Blend::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case Graphics::Blend::InvSrc_1_Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert Blend ({}) to D3D", magic_enum::enum_name(blend));
                logger->warn("Will return D3D12_BLEND_ZERO!");
            }
        }
    }

    return D3D12_BLEND_ZERO;
}

inline D3D12_BLEND_OP to_d3d_blend_op(Graphics::BlendOp operation) {
    switch (operation) {
        case Graphics::BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case Graphics::BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case Graphics::BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case Graphics::BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case Graphics::BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert BlendOp ({}) to D3D", magic_enum::enum_name(operation));
                logger->warn("Will return D3D12_BLEND_OP_ADD!");
            }
        }
    }
    return D3D12_BLEND_OP_ADD;
}

inline D3D12_LOGIC_OP to_d3d_logic_op(Graphics::LogicOp operation) {
    switch (operation) {
        case Graphics::LogicOp::Clear:
            return D3D12_LOGIC_OP_CLEAR;
        case Graphics::LogicOp::Set:
            return D3D12_LOGIC_OP_SET;
        case Graphics::LogicOp::Copy:
            return D3D12_LOGIC_OP_COPY;
        case Graphics::LogicOp::CopyInverted:
            return D3D12_LOGIC_OP_COPY_INVERTED;
        case Graphics::LogicOp::Noop:
            return D3D12_LOGIC_OP_NOOP;
        case Graphics::LogicOp::Invert:
            return D3D12_LOGIC_OP_INVERT;
        case Graphics::LogicOp::And:
            return D3D12_LOGIC_OP_AND;
        case Graphics::LogicOp::Nand:
            return D3D12_LOGIC_OP_NAND;
        case Graphics::LogicOp::Or:
            return D3D12_LOGIC_OP_OR;
        case Graphics::LogicOp::Nor:
            return D3D12_LOGIC_OP_NOR;
        case Graphics::LogicOp::Xor:
            return D3D12_LOGIC_OP_XOR;
        case Graphics::LogicOp::Equiv:
            return D3D12_LOGIC_OP_EQUIV;
        case Graphics::LogicOp::AndReverse:
            return D3D12_LOGIC_OP_AND_REVERSE;
        case Graphics::LogicOp::AndInverted:
            return D3D12_LOGIC_OP_AND_INVERTED;
        case Graphics::LogicOp::OrReverse:
            return D3D12_LOGIC_OP_OR_REVERSE;
        case Graphics::LogicOp::OrInverted:
            return D3D12_LOGIC_OP_OR_INVERTED;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert LogicOp ({}) to D3D", magic_enum::enum_name(operation));
                logger->warn("Will return D3D12_LOGIC_OP_CLEAR!");
            }
        }
    }
    return D3D12_LOGIC_OP_CLEAR;
}

inline D3D12_BLEND_DESC to_d3d_blend_desc(Graphics::BlendDescription desc) noexcept {
    D3D12_BLEND_DESC result{};
    result.AlphaToCoverageEnable  = desc.alpha_to_coverage_enable;
    result.IndependentBlendEnable = desc.independent_blend_enable;

    result.RenderTarget[0].BlendEnable           = desc.enable_blend;
    result.RenderTarget[0].LogicOpEnable         = desc.enable_logic_operation;
    result.RenderTarget[0].SrcBlend              = to_d3d_blend(desc.src_blend);
    result.RenderTarget[0].DestBlend             = to_d3d_blend(desc.dest_blend);
    result.RenderTarget[0].BlendOp               = to_d3d_blend_op(desc.blend_op);
    result.RenderTarget[0].SrcBlendAlpha         = to_d3d_blend(desc.src_blend_alpha);
    result.RenderTarget[0].DestBlendAlpha        = to_d3d_blend(desc.dest_blend_alpha);
    result.RenderTarget[0].BlendOpAlpha          = to_d3d_blend_op(desc.blend_op_alpha);
    result.RenderTarget[0].LogicOp               = to_d3d_logic_op(desc.logic_op);
    result.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    return result;
}

inline D3D12_FILL_MODE to_d3d_fill_mode(Graphics::FillMode mode) {
    switch (mode) {
        case Graphics::FillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        case Graphics::FillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert FillMode ({}) to D3D", magic_enum::enum_name(mode));
                logger->warn("Will return D3D12_FILL_MODE_SOLID!");
            }
        }
    }
    return D3D12_FILL_MODE_SOLID;
}

inline D3D12_CULL_MODE to_d3d_cull_mode(Graphics::CullMode mode) {
    switch (mode) {
        case Graphics::CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case Graphics::CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case Graphics::CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default: {
            auto logger = spdlog::get("GraphicsManager");
            if (logger) {
                logger->warn("Can not convert CullMode ({}) to D3D", magic_enum::enum_name(mode));
                logger->warn("Will return D3D12_CULL_MODE_NONE!");
            }
        }
    }
    return D3D12_CULL_MODE_NONE;
}

inline D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(Graphics::RasterizerDescription desc) noexcept {
    D3D12_RASTERIZER_DESC result = {};
    result.FillMode              = to_d3d_fill_mode(desc.fill_mode);
    result.CullMode              = to_d3d_cull_mode(desc.cull_mode);
    result.FrontCounterClockwise = desc.front_counter_clockwise;
    result.DepthBias             = desc.depth_bias;
    result.DepthBiasClamp        = desc.depth_bias_clamp;
    result.SlopeScaledDepthBias  = desc.slope_scaled_depth_bias;
    result.DepthClipEnable       = desc.depth_clip_enable;
    result.MultisampleEnable     = desc.multisample_enable;
    result.AntialiasedLineEnable = desc.antialiased_line_enable;
    result.ForcedSampleCount     = desc.forced_sample_count;
    result.ConservativeRaster    = desc.conservative_raster ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return result;
}

}  // namespace Hitagi::Graphics::backend::DX12