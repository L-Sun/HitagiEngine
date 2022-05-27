#pragma once
#include "d3d_pch.hpp"
#include <hitagi/resource/enums.hpp>
#include <hitagi/graphics/resource.hpp>

#include <dxgiformat.h>
#include <magic_enum.hpp>

// TODO change when integrate Vulkan
namespace hitagi::graphics::backend::DX12 {
inline const size_t align(size_t x, size_t a) {
    assert(((a - 1) & a) == 0 && "alignment is not a power of two");
    return (x + a - 1) & ~(a - 1);
}

inline auto hlsl_semantic_name(resource::VertexAttribute attr) noexcept {
    switch (attr) {
        case resource::VertexAttribute::Position:
            return "POSITION";
        case resource::VertexAttribute::Normal:
            return "NORMAL";
        case resource::VertexAttribute::Tangent:
            return "TANGENT";
        case resource::VertexAttribute::Bitangent:
            return "BITANGENT";
        case resource::VertexAttribute::Color0:
        case resource::VertexAttribute::Color1:
        case resource::VertexAttribute::Color2:
        case resource::VertexAttribute::Color3:
            return "COLOR";
        case resource::VertexAttribute::UV0:
        case resource::VertexAttribute::UV1:
        case resource::VertexAttribute::UV2:
        case resource::VertexAttribute::UV3:
            return "UV";
        case resource::VertexAttribute::BlendIndex:
            return "BLENDINDEX";
        case resource::VertexAttribute::BlendWeight:
            return "BLENDWEIGHT";
        default:
            return "PSIZE";
    }
}

inline auto hlsl_semantic_index(resource::VertexAttribute attr) noexcept {
    switch (attr) {
        case resource::VertexAttribute::Position:
        case resource::VertexAttribute::Normal:
        case resource::VertexAttribute::Tangent:
        case resource::VertexAttribute::Bitangent:
        case resource::VertexAttribute::Color0:
            return 0;
        case resource::VertexAttribute::Color1:
            return 1;
        case resource::VertexAttribute::Color2:
            return 2;
        case resource::VertexAttribute::Color3:
            return 3;
        case resource::VertexAttribute::UV0:
            return 0;
        case resource::VertexAttribute::UV1:
            return 1;
        case resource::VertexAttribute::UV2:
            return 2;
        case resource::VertexAttribute::UV3:
            return 3;
        case resource::VertexAttribute::BlendIndex:
        case resource::VertexAttribute::BlendWeight:
            return 0;
        case resource::VertexAttribute::Custom0:
            return 0;
        case resource::VertexAttribute::Custom1:
            return 1;
    }
}

inline auto hlsl_semantic_format(resource::VertexAttribute attr) noexcept {
    switch (attr) {
        case resource::VertexAttribute::Position:
        case resource::VertexAttribute::Normal:
        case resource::VertexAttribute::Tangent:
        case resource::VertexAttribute::Bitangent:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case resource::VertexAttribute::Color0:
        case resource::VertexAttribute::Color1:
        case resource::VertexAttribute::Color2:
        case resource::VertexAttribute::Color3:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case resource::VertexAttribute::UV0:
        case resource::VertexAttribute::UV1:
        case resource::VertexAttribute::UV2:
        case resource::VertexAttribute::UV3:
            return DXGI_FORMAT_R32G32_FLOAT;
        case resource::VertexAttribute::BlendIndex:
            return DXGI_FORMAT_R32_UINT;
        case resource::VertexAttribute::BlendWeight:
            return DXGI_FORMAT_R32_FLOAT;
        default:
            return DXGI_FORMAT_R32_FLOAT;
    }
}

inline DXGI_FORMAT to_dxgi_format(graphics::Format format) noexcept { return static_cast<DXGI_FORMAT>(format); }

inline graphics::Format to_format(DXGI_FORMAT format) noexcept { return static_cast<graphics::Format>(format); }

inline D3D12_TEXTURE_ADDRESS_MODE to_d3d_texture_address_mode(graphics::TextureAddressMode mode) {
    switch (mode) {
        case graphics::TextureAddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case graphics::TextureAddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case graphics::TextureAddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case graphics::TextureAddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case graphics::TextureAddressMode::MirrorOnce:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

inline D3D12_COMPARISON_FUNC to_d3d_comp_func(graphics::ComparisonFunc func) {
    switch (func) {
        case graphics::ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case graphics::ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case graphics::ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case graphics::ComparisonFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case graphics::ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case graphics::ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case graphics::ComparisonFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case graphics::ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
    }
}

// TODO change when integrate Vulkan
inline D3D12_FILTER to_d3d_filter(graphics::Filter filter) noexcept {
    return static_cast<D3D12_FILTER>(filter);
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE to_dx_topology_type(resource::PrimitiveType type) noexcept {
    switch (type) {
        case resource::PrimitiveType::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case resource::PrimitiveType::LineList:
        case resource::PrimitiveType::LineStrip:
        case resource::PrimitiveType::LineListAdjacency:
        case resource::PrimitiveType::LineStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case resource::PrimitiveType::TriangleList:
        case resource::PrimitiveType::TriangleStrip:
        case resource::PrimitiveType::TriangleListAdjacency:
        case resource::PrimitiveType::TriangleStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY to_dx_topology(resource::PrimitiveType type) noexcept {
    switch (type) {
        case resource::PrimitiveType::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case resource::PrimitiveType::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case resource::PrimitiveType::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case resource::PrimitiveType::LineListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case resource::PrimitiveType::LineStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case resource::PrimitiveType::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case resource::PrimitiveType::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case resource::PrimitiveType::TriangleListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case resource::PrimitiveType::TriangleStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
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

inline D3D12_SAMPLER_DESC to_d3d_sampler_desc(graphics::SamplerDesc desc) noexcept {
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

inline D3D12_SHADER_VISIBILITY to_d3d_shader_visibility(graphics::ShaderVisibility visibility) {
    switch (visibility) {
        case graphics::ShaderVisibility::All:
            return D3D12_SHADER_VISIBILITY_ALL;
        case graphics::ShaderVisibility::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case graphics::ShaderVisibility::Hull:
            return D3D12_SHADER_VISIBILITY_HULL;
        case graphics::ShaderVisibility::Domain:
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case graphics::ShaderVisibility::Geometry:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case graphics::ShaderVisibility::Pixel:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        default:
            return D3D12_SHADER_VISIBILITY_ALL;
    }
}

inline D3D12_BLEND to_d3d_blend(graphics::Blend blend) {
    switch (blend) {
        case graphics::Blend::Zero:
            return D3D12_BLEND_ZERO;
        case graphics::Blend::One:
            return D3D12_BLEND_ONE;
        case graphics::Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case graphics::Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case graphics::Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case graphics::Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case graphics::Blend::DestAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case graphics::Blend::InvDestAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case graphics::Blend::DestColor:
            return D3D12_BLEND_DEST_COLOR;
        case graphics::Blend::InvDestColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case graphics::Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case graphics::Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case graphics::Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case graphics::Blend::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case graphics::Blend::InvSrc_1_Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case graphics::Blend::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case graphics::Blend::InvSrc_1_Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            return D3D12_BLEND_ZERO;
    }
}

inline D3D12_BLEND_OP to_d3d_blend_op(graphics::BlendOp operation) {
    switch (operation) {
        case graphics::BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case graphics::BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case graphics::BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case graphics::BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case graphics::BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

inline D3D12_LOGIC_OP to_d3d_logic_op(graphics::LogicOp operation) {
    switch (operation) {
        case graphics::LogicOp::Clear:
            return D3D12_LOGIC_OP_CLEAR;
        case graphics::LogicOp::Set:
            return D3D12_LOGIC_OP_SET;
        case graphics::LogicOp::Copy:
            return D3D12_LOGIC_OP_COPY;
        case graphics::LogicOp::CopyInverted:
            return D3D12_LOGIC_OP_COPY_INVERTED;
        case graphics::LogicOp::Noop:
            return D3D12_LOGIC_OP_NOOP;
        case graphics::LogicOp::Invert:
            return D3D12_LOGIC_OP_INVERT;
        case graphics::LogicOp::And:
            return D3D12_LOGIC_OP_AND;
        case graphics::LogicOp::Nand:
            return D3D12_LOGIC_OP_NAND;
        case graphics::LogicOp::Or:
            return D3D12_LOGIC_OP_OR;
        case graphics::LogicOp::Nor:
            return D3D12_LOGIC_OP_NOR;
        case graphics::LogicOp::Xor:
            return D3D12_LOGIC_OP_XOR;
        case graphics::LogicOp::Equiv:
            return D3D12_LOGIC_OP_EQUIV;
        case graphics::LogicOp::AndReverse:
            return D3D12_LOGIC_OP_AND_REVERSE;
        case graphics::LogicOp::AndInverted:
            return D3D12_LOGIC_OP_AND_INVERTED;
        case graphics::LogicOp::OrReverse:
            return D3D12_LOGIC_OP_OR_REVERSE;
        case graphics::LogicOp::OrInverted:
            return D3D12_LOGIC_OP_OR_INVERTED;
        default:
            return D3D12_LOGIC_OP_CLEAR;
    }
}

inline D3D12_BLEND_DESC to_d3d_blend_desc(graphics::BlendDescription desc) noexcept {
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

inline D3D12_FILL_MODE to_d3d_fill_mode(graphics::FillMode mode) {
    switch (mode) {
        case graphics::FillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        case graphics::FillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        default:
            return D3D12_FILL_MODE_SOLID;
    }
}

inline D3D12_CULL_MODE to_d3d_cull_mode(graphics::CullMode mode) {
    switch (mode) {
        case graphics::CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case graphics::CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case graphics::CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
    }
}

inline D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(graphics::RasterizerDescription desc) noexcept {
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

}  // namespace hitagi::graphics::backend::DX12