#pragma once
#include "d3d_pch.hpp"
#include "descriptor_allocator.hpp"

#include <hitagi/graphics/gpu_resource.hpp>

#include <d3d12.h>
#include <dxgiformat.h>
#include <magic_enum.hpp>
#include <stdexcept>

// TODO change when integrate Vulkan
namespace hitagi::gfx::backend::DX12 {
constexpr auto range_type_to_descriptor_type(D3D12_DESCRIPTOR_RANGE_TYPE type) {
    switch (type) {
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            return Descriptor::Type::SRV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            return Descriptor::Type::UAV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            return Descriptor::Type::CBV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            return Descriptor::Type::Sampler;
    }
}

inline auto root_parameter_type_to_descriptor_type(D3D12_ROOT_PARAMETER_TYPE type) {
    switch (type) {
        case D3D12_ROOT_PARAMETER_TYPE_CBV:
            return Descriptor::Type::CBV;
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
            return Descriptor::Type::SRV;
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
            return Descriptor::Type::UAV;
        default:
            throw std::invalid_argument("only root descriptor type can be cast!");
    }
}

inline constexpr auto descriptor_type_to_heap_type(Descriptor::Type type) {
    switch (type) {
        case Descriptor::Type::CBV:
        case Descriptor::Type::UAV:
        case Descriptor::Type::SRV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        case Descriptor::Type::Sampler:
            return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        case Descriptor::Type::DSV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        case Descriptor::Type::RTV:
            return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    }
}

inline auto heap_type_to_descriptor_types(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    switch (type) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            return std::pmr::vector<Descriptor::Type>{
                Descriptor::Type::CBV,
                Descriptor::Type::UAV,
                Descriptor::Type::SRV};
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            return std::pmr::vector<Descriptor::Type>{Descriptor::Type::Sampler};
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
            return std::pmr::vector<Descriptor::Type>{Descriptor::Type::DSV};
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            return std::pmr::vector<Descriptor::Type>{Descriptor::Type::RTV};
        default:
            throw std::invalid_argument("Unkown descriptor heap type");
    }
}

inline constexpr auto hlsl_semantic_name(resource::VertexAttribute attr) {
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
            return "TEXCOORD";
        case resource::VertexAttribute::BlendIndex:
            return "BLENDINDEX";
        case resource::VertexAttribute::BlendWeight:
            return "BLENDWEIGHT";
        default:
            return "PSIZE";
    }
}

inline constexpr auto hlsl_semantic_name(std::string_view semantic_name) {
    if (semantic_name == "POSITION") return "POSITION";
    if (semantic_name == "NORMAL") return "NORMAL";
    if (semantic_name == "TANGENT") return "TANGENT";
    if (semantic_name == "BITANGENT") return "BITANGENT";
    if (semantic_name == "COLOR") return "COLOR";
    if (semantic_name == "TEXCOORD") return "TEXCOORD";
    if (semantic_name == "BLENDINDEX") return "BLENDINDEX";
    if (semantic_name == "BLENDWEIGHT") return "BLENDWEIGHT";

    return "PSIZE";
}

inline constexpr auto hlsl_semantic_index(resource::VertexAttribute attr) noexcept {
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

inline constexpr auto hlsl_semantic_format(resource::VertexAttribute attr) noexcept {
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

inline constexpr DXGI_FORMAT to_dxgi_format(resource::Format format) noexcept { return static_cast<DXGI_FORMAT>(format); }
inline constexpr DXGI_FORMAT to_dxgi_format(D3D_REGISTER_COMPONENT_TYPE type, BYTE mask) {
    std::size_t num_components = std::countr_one(mask);
    switch (num_components) {
        case 1: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return DXGI_FORMAT_UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return DXGI_FORMAT_R32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return DXGI_FORMAT_R32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return DXGI_FORMAT_R32_FLOAT;
            }
        }
        case 2: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return DXGI_FORMAT_UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return DXGI_FORMAT_R32G32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return DXGI_FORMAT_R32G32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return DXGI_FORMAT_R32G32_FLOAT;
            }
        }
        case 3: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return DXGI_FORMAT_UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return DXGI_FORMAT_R32G32B32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return DXGI_FORMAT_R32G32B32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return DXGI_FORMAT_R32G32B32_FLOAT;
            }
        }
        case 4: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return DXGI_FORMAT_UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return DXGI_FORMAT_R32G32B32A32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return DXGI_FORMAT_R32G32B32A32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
        }
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}
inline constexpr DXGI_FORMAT to_srv_format(DXGI_FORMAT format) noexcept {
    switch (format) {
        case DXGI_FORMAT_D16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case DXGI_FORMAT_D32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default:
            return format;
    }
}

inline constexpr resource::Format to_format(DXGI_FORMAT format) noexcept { return static_cast<resource::Format>(format); }

inline constexpr D3D12_TEXTURE_ADDRESS_MODE to_d3d_texture_address_mode(resource::TextureAddressMode mode) {
    switch (mode) {
        case resource::TextureAddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case resource::TextureAddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case resource::TextureAddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case resource::TextureAddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case resource::TextureAddressMode::MirrorOnce:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

inline constexpr D3D12_COMPARISON_FUNC to_d3d_comp_func(resource::ComparisonFunc func) {
    switch (func) {
        case resource::ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case resource::ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case resource::ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case resource::ComparisonFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case resource::ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case resource::ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case resource::ComparisonFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case resource::ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
    }
}

// TODO change when integrate Vulkan
inline D3D12_FILTER to_d3d_filter(resource::Filter filter) noexcept {
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

inline DXGI_FORMAT index_type_to_dxgi_format(resource::IndexType type) noexcept {
    switch (type) {
        case resource::IndexType::UINT16:
            return DXGI_FORMAT_R16_UINT;
        case resource::IndexType::UINT32:
            return DXGI_FORMAT_R32_UINT;
    }
}

inline D3D12_SAMPLER_DESC to_d3d_sampler_desc(const resource::Sampler& sampler) noexcept {
    D3D12_SAMPLER_DESC result{};
    result.AddressU       = to_d3d_texture_address_mode(sampler.address_u);
    result.AddressV       = to_d3d_texture_address_mode(sampler.address_v);
    result.AddressW       = to_d3d_texture_address_mode(sampler.address_w);
    result.BorderColor[0] = sampler.border_color.r;
    result.BorderColor[1] = sampler.border_color.g;
    result.BorderColor[2] = sampler.border_color.b;
    result.BorderColor[3] = sampler.border_color.a;
    result.ComparisonFunc = to_d3d_comp_func(sampler.comp_func);
    result.Filter         = to_d3d_filter(sampler.filter);
    result.MaxAnisotropy  = sampler.max_anisotropy;
    result.MaxLOD         = sampler.max_lod;
    result.MinLOD         = sampler.min_lod;
    result.MipLODBias     = sampler.mip_lod_bias;

    return result;
}

inline D3D12_BLEND to_d3d_blend(gfx::Blend blend) {
    switch (blend) {
        case gfx::Blend::Zero:
            return D3D12_BLEND_ZERO;
        case gfx::Blend::One:
            return D3D12_BLEND_ONE;
        case gfx::Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case gfx::Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case gfx::Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case gfx::Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case gfx::Blend::DestAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case gfx::Blend::InvDestAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case gfx::Blend::DestColor:
            return D3D12_BLEND_DEST_COLOR;
        case gfx::Blend::InvDestColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case gfx::Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case gfx::Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case gfx::Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case gfx::Blend::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case gfx::Blend::InvSrc_1_Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case gfx::Blend::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case gfx::Blend::InvSrc_1_Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            return D3D12_BLEND_ZERO;
    }
}

inline D3D12_BLEND_OP to_d3d_blend_op(gfx::BlendOp operation) {
    switch (operation) {
        case gfx::BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case gfx::BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case gfx::BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case gfx::BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case gfx::BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

inline D3D12_LOGIC_OP to_d3d_logic_op(gfx::LogicOp operation) {
    switch (operation) {
        case gfx::LogicOp::Clear:
            return D3D12_LOGIC_OP_CLEAR;
        case gfx::LogicOp::Set:
            return D3D12_LOGIC_OP_SET;
        case gfx::LogicOp::Copy:
            return D3D12_LOGIC_OP_COPY;
        case gfx::LogicOp::CopyInverted:
            return D3D12_LOGIC_OP_COPY_INVERTED;
        case gfx::LogicOp::Noop:
            return D3D12_LOGIC_OP_NOOP;
        case gfx::LogicOp::Invert:
            return D3D12_LOGIC_OP_INVERT;
        case gfx::LogicOp::And:
            return D3D12_LOGIC_OP_AND;
        case gfx::LogicOp::Nand:
            return D3D12_LOGIC_OP_NAND;
        case gfx::LogicOp::Or:
            return D3D12_LOGIC_OP_OR;
        case gfx::LogicOp::Nor:
            return D3D12_LOGIC_OP_NOR;
        case gfx::LogicOp::Xor:
            return D3D12_LOGIC_OP_XOR;
        case gfx::LogicOp::Equiv:
            return D3D12_LOGIC_OP_EQUIV;
        case gfx::LogicOp::AndReverse:
            return D3D12_LOGIC_OP_AND_REVERSE;
        case gfx::LogicOp::AndInverted:
            return D3D12_LOGIC_OP_AND_INVERTED;
        case gfx::LogicOp::OrReverse:
            return D3D12_LOGIC_OP_OR_REVERSE;
        case gfx::LogicOp::OrInverted:
            return D3D12_LOGIC_OP_OR_INVERTED;
        default:
            return D3D12_LOGIC_OP_CLEAR;
    }
}

inline D3D12_BLEND_DESC to_d3d_blend_desc(gfx::BlendDescription desc) noexcept {
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

inline D3D12_FILL_MODE to_d3d_fill_mode(gfx::FillMode mode) {
    switch (mode) {
        case gfx::FillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        case gfx::FillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        default:
            return D3D12_FILL_MODE_SOLID;
    }
}

inline D3D12_CULL_MODE to_d3d_cull_mode(gfx::CullMode mode) {
    switch (mode) {
        case gfx::CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case gfx::CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case gfx::CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
    }
}

inline D3D12_RASTERIZER_DESC to_d3d_rasterizer_desc(gfx::RasterizerDescription desc) noexcept {
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

}  // namespace hitagi::gfx::backend::DX12