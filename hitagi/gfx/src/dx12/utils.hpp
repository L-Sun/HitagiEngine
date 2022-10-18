#pragma once
#include "resource_binder.hpp"
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/command_context.hpp>

#include <d3d12.h>
#include <comdef.h>
#include <d3dcommon.h>

namespace hitagi::gfx {

inline std::string HrToString(HRESULT hr) {
    _com_error err(hr);
    return {err.ErrorMessage()};
}

class HrException : public std::runtime_error {
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }

private:
    const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        throw HrException(hr);
    }
}

inline constexpr auto to_d3d_command_type(CommandType type) noexcept {
    switch (type) {
        case CommandType::Graphics:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case CommandType::Compute:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case CommandType::Copy:
            return D3D12_COMMAND_LIST_TYPE_COPY;
    }
}

inline constexpr auto to_dxgi_format(Format format) noexcept {
    switch (format) {
        case Format::UNKNOWN:
            return DXGI_FORMAT_UNKNOWN;
        case Format::R32G32B32A32_TYPELESS:
            return DXGI_FORMAT_R32G32B32A32_TYPELESS;
        case Format::R32G32B32A32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R32G32B32A32_UINT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case Format::R32G32B32A32_SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case Format::R32G32B32_TYPELESS:
            return DXGI_FORMAT_R32G32B32_TYPELESS;
        case Format::R32G32B32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case Format::R32G32B32_UINT:
            return DXGI_FORMAT_R32G32B32_UINT;
        case Format::R32G32B32_SINT:
            return DXGI_FORMAT_R32G32B32_SINT;
        case Format::R16G16B16A16_TYPELESS:
            return DXGI_FORMAT_R16G16B16A16_TYPELESS;
        case Format::R16G16B16A16_FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::R16G16B16A16_UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case Format::R16G16B16A16_UINT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case Format::R16G16B16A16_SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case Format::R16G16B16A16_SINT:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case Format::R32G32_TYPELESS:
            return DXGI_FORMAT_R32G32_TYPELESS;
        case Format::R32G32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;
        case Format::R32G32_UINT:
            return DXGI_FORMAT_R32G32_UINT;
        case Format::R32G32_SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case Format::R32G8X24_TYPELESS:
            return DXGI_FORMAT_R32G8X24_TYPELESS;
        case Format::D32_FLOAT_S8X24_UINT:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case Format::R32_FLOAT_X8X24_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        case Format::X32_TYPELESS_G8X24_UINT:
            return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
        case Format::R10G10B10A2_TYPELESS:
            return DXGI_FORMAT_R10G10B10A2_TYPELESS;
        case Format::R10G10B10A2_UNORM:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case Format::R10G10B10A2_UINT:
            return DXGI_FORMAT_R10G10B10A2_UINT;
        case Format::R11G11B10_FLOAT:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case Format::R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case Format::R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case Format::R8G8B8A8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case Format::R8G8B8A8_SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case Format::R8G8B8A8_SINT:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case Format::R16G16_TYPELESS:
            return DXGI_FORMAT_R16G16_TYPELESS;
        case Format::R16G16_FLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;
        case Format::R16G16_UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case Format::R16G16_UINT:
            return DXGI_FORMAT_R16G16_UINT;
        case Format::R16G16_SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case Format::R16G16_SINT:
            return DXGI_FORMAT_R16G16_SINT;
        case Format::R32_TYPELESS:
            return DXGI_FORMAT_R32_TYPELESS;
        case Format::D32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case Format::R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;
        case Format::R32_UINT:
            return DXGI_FORMAT_R32_UINT;
        case Format::R32_SINT:
            return DXGI_FORMAT_R32_SINT;
        case Format::R24G8_TYPELESS:
            return DXGI_FORMAT_R24G8_TYPELESS;
        case Format::D24_UNORM_S8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case Format::R24_UNORM_X8_TYPELESS:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case Format::X24_TYPELESS_G8_UINT:
            return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
        case Format::R8G8_TYPELESS:
            return DXGI_FORMAT_R8G8_TYPELESS;
        case Format::R8G8_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case Format::R8G8_UINT:
            return DXGI_FORMAT_R8G8_UINT;
        case Format::R8G8_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case Format::R8G8_SINT:
            return DXGI_FORMAT_R8G8_SINT;
        case Format::R16_TYPELESS:
            return DXGI_FORMAT_R16_TYPELESS;
        case Format::R16_FLOAT:
            return DXGI_FORMAT_R16_FLOAT;
        case Format::D16_UNORM:
            return DXGI_FORMAT_D16_UNORM;
        case Format::R16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case Format::R16_UINT:
            return DXGI_FORMAT_R16_UINT;
        case Format::R16_SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case Format::R16_SINT:
            return DXGI_FORMAT_R16_SINT;
        case Format::R8_TYPELESS:
            return DXGI_FORMAT_R8_TYPELESS;
        case Format::R8_UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case Format::R8_UINT:
            return DXGI_FORMAT_R8_UINT;
        case Format::R8_SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case Format::R8_SINT:
            return DXGI_FORMAT_R8_SINT;
        case Format::A8_UNORM:
            return DXGI_FORMAT_A8_UNORM;
        case Format::R1_UNORM:
            return DXGI_FORMAT_R1_UNORM;
        case Format::R9G9B9E5_SHAREDEXP:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case Format::R8G8_B8G8_UNORM:
            return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case Format::G8R8_G8B8_UNORM:
            return DXGI_FORMAT_G8R8_G8B8_UNORM;
        case Format::BC1_TYPELESS:
            return DXGI_FORMAT_BC1_TYPELESS;
        case Format::BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM;
        case Format::BC1_UNORM_SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case Format::BC2_TYPELESS:
            return DXGI_FORMAT_BC2_TYPELESS;
        case Format::BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM;
        case Format::BC2_UNORM_SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case Format::BC3_TYPELESS:
            return DXGI_FORMAT_BC3_TYPELESS;
        case Format::BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM;
        case Format::BC3_UNORM_SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case Format::BC4_TYPELESS:
            return DXGI_FORMAT_BC4_TYPELESS;
        case Format::BC4_UNORM:
            return DXGI_FORMAT_BC4_UNORM;
        case Format::BC4_SNORM:
            return DXGI_FORMAT_BC4_SNORM;
        case Format::BC5_TYPELESS:
            return DXGI_FORMAT_BC5_TYPELESS;
        case Format::BC5_UNORM:
            return DXGI_FORMAT_BC5_UNORM;
        case Format::BC5_SNORM:
            return DXGI_FORMAT_BC5_SNORM;
        case Format::B5G6R5_UNORM:
            return DXGI_FORMAT_B5G6R5_UNORM;
        case Format::B5G5R5A1_UNORM:
            return DXGI_FORMAT_B5G5R5A1_UNORM;
        case Format::B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8X8_UNORM:
            return DXGI_FORMAT_B8G8R8X8_UNORM;
        case Format::R10G10B10_XR_BIAS_A2_UNORM:
            return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
        case Format::B8G8R8A8_TYPELESS:
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        case Format::B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case Format::B8G8R8X8_TYPELESS:
            return DXGI_FORMAT_B8G8R8X8_TYPELESS;
        case Format::B8G8R8X8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case Format::BC6H_TYPELESS:
            return DXGI_FORMAT_BC6H_TYPELESS;
        case Format::BC6H_UF16:
            return DXGI_FORMAT_BC6H_UF16;
        case Format::BC6H_SF16:
            return DXGI_FORMAT_BC6H_SF16;
        case Format::BC7_TYPELESS:
            return DXGI_FORMAT_BC7_TYPELESS;
        case Format::BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM;
        case Format::BC7_UNORM_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
    }
}

inline constexpr auto to_d3d_primitive_topology_type(PrimitiveTopology type) noexcept {
    switch (type) {
        case PrimitiveTopology::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::LineList:
        case PrimitiveTopology::LineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::TriangleList:
        case PrimitiveTopology::TriangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

inline constexpr auto to_d3d_primitive_topology(PrimitiveTopology type) {
    switch (type) {
        case PrimitiveTopology::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

inline constexpr auto to_d3d_address_mode(Sampler::AddressMode mode) noexcept {
    switch (mode) {
        case Sampler::AddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case Sampler::AddressMode::Repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case Sampler::AddressMode::MirrorRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
}

inline constexpr auto to_d3d_filter_type(Sampler::FilterMode mode) noexcept {
    switch (mode) {
        case Sampler::FilterMode::Point:
            return D3D12_FILTER_TYPE_POINT;
        case Sampler::FilterMode::Linear:
            return D3D12_FILTER_TYPE_LINEAR;
    }
}

inline constexpr auto to_d3d_compare_function(CompareFunction func) noexcept {
    switch (func) {
        case CompareFunction::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case CompareFunction::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case CompareFunction::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareFunction::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareFunction::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case CompareFunction::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareFunction::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareFunction::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

inline constexpr auto to_d3d_blend(Blend blend) noexcept {
    switch (blend) {
        case Blend::Zero:
            return D3D12_BLEND_ZERO;
        case Blend::One:
            return D3D12_BLEND_ONE;
        case Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case Blend::DestAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case Blend::InvDestAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case Blend::DestColor:
            return D3D12_BLEND_DEST_COLOR;
        case Blend::InvDestColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case Blend::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case Blend::InvSrc_1_Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case Blend::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case Blend::InvSrc_1_Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            return D3D12_BLEND_ZERO;
    }
}

inline constexpr auto to_d3d_blend_op(BlendOp operation) noexcept {
    switch (operation) {
        case BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

inline constexpr auto to_d3d_logic_op(LogicOp operation) noexcept {
    switch (operation) {
        case LogicOp::Clear:
            return D3D12_LOGIC_OP_CLEAR;
        case LogicOp::Set:
            return D3D12_LOGIC_OP_SET;
        case LogicOp::Copy:
            return D3D12_LOGIC_OP_COPY;
        case LogicOp::CopyInverted:
            return D3D12_LOGIC_OP_COPY_INVERTED;
        case LogicOp::Noop:
            return D3D12_LOGIC_OP_NOOP;
        case LogicOp::Invert:
            return D3D12_LOGIC_OP_INVERT;
        case LogicOp::And:
            return D3D12_LOGIC_OP_AND;
        case LogicOp::Nand:
            return D3D12_LOGIC_OP_NAND;
        case LogicOp::Or:
            return D3D12_LOGIC_OP_OR;
        case LogicOp::Nor:
            return D3D12_LOGIC_OP_NOR;
        case LogicOp::Xor:
            return D3D12_LOGIC_OP_XOR;
        case LogicOp::Equiv:
            return D3D12_LOGIC_OP_EQUIV;
        case LogicOp::AndReverse:
            return D3D12_LOGIC_OP_AND_REVERSE;
        case LogicOp::AndInverted:
            return D3D12_LOGIC_OP_AND_INVERTED;
        case LogicOp::OrReverse:
            return D3D12_LOGIC_OP_OR_REVERSE;
        case LogicOp::OrInverted:
            return D3D12_LOGIC_OP_OR_INVERTED;
        default:
            return D3D12_LOGIC_OP_CLEAR;
    }
}

inline constexpr auto to_d3d_blend_desc(BlendDescription desc) noexcept {
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

inline constexpr auto to_d3d_fill_mode(FillMode mode) noexcept {
    switch (mode) {
        case FillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        case FillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        default:
            return D3D12_FILL_MODE_SOLID;
    }
}

inline constexpr auto to_d3d_cull_mode(CullMode mode) noexcept {
    switch (mode) {
        case CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
    }
}

inline constexpr auto to_d3d_rasterizer_desc(RasterizerDescription desc) noexcept {
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

// for holding object life time
struct DX12InputLayout {
    std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> attributes;
    D3D12_INPUT_LAYOUT_DESC                    desc;
};

inline constexpr auto to_d3d_input_layout_desc(const InputLayout& layout) noexcept {
    DX12InputLayout result;
    for (const auto& attribute : layout) {
        result.attributes.emplace_back(D3D12_INPUT_ELEMENT_DESC{
            .SemanticName         = attribute.semantic_name.data(),
            .SemanticIndex        = attribute.semantic_index,
            .Format               = to_dxgi_format(attribute.format),
            .InputSlot            = attribute.slot,
            .AlignedByteOffset    = attribute.aligned_offset,
            .InputSlotClass       = attribute.per_instance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = attribute.instance_data_step_rate,
        });
    }
    result.desc.pInputElementDescs = result.attributes.data();
    result.desc.NumElements        = result.attributes.size();

    return result;
}

inline constexpr auto to_shader_model_compile_flag(Shader::Type type, D3D_SHADER_MODEL version) noexcept {
    std::pmr::wstring result;
    switch (type) {
        case Shader::Type::Vertex:
            result = L"vs_";
            break;
        case Shader::Type::Pixel:
            result = L"ps_";
            break;
        case Shader::Type::Geometry:
            result = L"gs_";
            break;
        case Shader::Type::Compute:
            result = L"cs_";
            break;
    }
    switch (version) {
        case D3D_SHADER_MODEL_5_1:
            result += L"5_1";
            break;
        case D3D_SHADER_MODEL_6_0:
            result += L"6_0";
            break;
        case D3D_SHADER_MODEL_6_1:
            result += L"6_1";
            break;
        case D3D_SHADER_MODEL_6_2:
            result += L"6_2";
            break;
        case D3D_SHADER_MODEL_6_3:
            result += L"6_3";
            break;
        case D3D_SHADER_MODEL_6_4:
            result += L"6_4";
            break;
        case D3D_SHADER_MODEL_6_5:
            result += L"6_5";
            break;
        case D3D_SHADER_MODEL_6_6:
            result += L"6_6";
            break;
        case D3D_SHADER_MODEL_6_7:
            result += L"6_7";
            break;
    }
    return result;
}

inline constexpr auto to_d3d_view_port(const ViewPort& view_port) noexcept {
    return D3D12_VIEWPORT{
        .TopLeftX = view_port.x,
        .TopLeftY = view_port.y,
        .Width    = view_port.width,
        .Height   = view_port.height,
        .MinDepth = view_port.min_depth,
        .MaxDepth = view_port.max_depth,
    };
}

inline constexpr auto to_d3d_rect(const Rect& rect) noexcept {
    return D3D12_RECT{
        .left   = static_cast<LONG>(rect.x),
        .top    = static_cast<LONG>(rect.y),
        .right  = static_cast<LONG>(rect.x + rect.width),
        .bottom = static_cast<LONG>(rect.y + rect.height),
    };
}

inline constexpr auto to_d3d_srv_desc(const TextureView::Desc& desc) noexcept {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format                  = to_dxgi_format(desc.format),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    // Dimension detect
    {
        if (desc.textuer.desc.height == 1 && desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            } else {
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            }

        } else if (desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                if (desc.is_cube) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                } else if (desc.textuer.desc.sample_count > 1) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                } else {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                }
            } else {
                if (desc.is_cube) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                } else if (desc.textuer.desc.sample_count > 1) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                } else {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                }
            }
        } else {
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        }
    }

    // fill data
    {
        switch (srv_desc.ViewDimension) {
            case D3D12_SRV_DIMENSION_TEXTURE1D: {
                srv_desc.Texture1D = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
                srv_desc.Texture1DArray = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .FirstArraySlice     = desc.base_array_layer,
                    .ArraySize           = desc.array_layer_count,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2D: {
                srv_desc.Texture2D = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .PlaneSlice          = 0,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
                srv_desc.Texture2DArray = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .FirstArraySlice     = desc.base_array_layer,
                    .ArraySize           = desc.array_layer_count,
                    .PlaneSlice          = 0,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
                srv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
                srv_desc.Texture2DMSArray = {
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE3D: {
                srv_desc.Texture3D = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURECUBE: {
                srv_desc.TextureCube = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
                srv_desc.TextureCubeArray = {
                    .MostDetailedMip     = desc.base_mip_level,
                    .MipLevels           = desc.mip_level_count,
                    .First2DArrayFace    = desc.base_array_layer,
                    .NumCubes            = desc.array_layer_count,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            default: {
                // TODO raytracing
            }
        }
    }

    return srv_desc;
}

inline constexpr auto to_d3d_uav_desc(const TextureView::Desc& desc) noexcept {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.textuer.desc.height == 1 && desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            } else {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            }
        } else if (desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            } else {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            }
        } else {
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        }
    }

    // fill data
    {
        switch (uav_desc.ViewDimension) {
            case D3D12_UAV_DIMENSION_TEXTURE1D: {
                uav_desc.Texture1D = {
                    .MipSlice = desc.base_mip_level,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
                uav_desc.Texture1DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE2D: {
                uav_desc.Texture2D = {
                    .MipSlice   = desc.base_mip_level,
                    .PlaneSlice = 0,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
                uav_desc.Texture2DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                    .PlaneSlice      = 0,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE3D: {
                uav_desc.Texture3D = {
                    .MipSlice    = desc.base_mip_level,
                    .FirstWSlice = 0,
                    .WSize       = desc.textuer.desc.depth,
                };
            } break;
            default: {
            }
        }
    }

    return uav_desc;
}

inline auto to_d3d_rtv_desc(const TextureView::Desc& desc) noexcept {
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.textuer.desc.height == 1 && desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            } else {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            }

        } else if (desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                if (desc.textuer.desc.sample_count > 1) {
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                } else {
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                }
            } else {
                if (desc.textuer.desc.sample_count > 1) {
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                } else {
                    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                }
            }
        } else {
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        }
    }

    // fill data
    {
        switch (rtv_desc.ViewDimension) {
            case D3D12_RTV_DIMENSION_TEXTURE1D: {
                rtv_desc.Texture1D = {
                    .MipSlice = desc.base_mip_level,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: {
                rtv_desc.Texture1DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2D: {
                rtv_desc.Texture2D = {
                    .MipSlice   = desc.base_mip_level,
                    .PlaneSlice = 0,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: {
                rtv_desc.Texture2DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                    .PlaneSlice      = 0,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2DMS: {
                rtv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: {
                rtv_desc.Texture2DMSArray = {
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE3D: {
                rtv_desc.Texture3D = {
                    .MipSlice = desc.base_mip_level,
                };
            } break;
            default: {
                // Nothing to do
            }
        }
    }

    return rtv_desc;
}

inline auto to_d3d_dsv_desc(const TextureView::Desc& desc) noexcept {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.textuer.desc.height == 1 && desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            } else {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            }

        } else if (desc.textuer.desc.depth == 1) {
            if (desc.textuer.desc.array_size == 1) {
                if (desc.textuer.desc.sample_count > 1) {
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                } else {
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                }
            } else {
                if (desc.textuer.desc.sample_count > 1) {
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                } else {
                    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                }
            }
        } else {
            dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_UNKNOWN;
        }
    }

    // fill data
    {
        switch (dsv_desc.ViewDimension) {
            case D3D12_DSV_DIMENSION_TEXTURE1D: {
                dsv_desc.Texture1D = {
                    .MipSlice = desc.base_mip_level,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: {
                dsv_desc.Texture1DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2D: {
                dsv_desc.Texture2D = {
                    .MipSlice = desc.base_mip_level,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: {
                dsv_desc.Texture2DArray = {
                    .MipSlice        = desc.base_mip_level,
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2DMS: {
                dsv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: {
                dsv_desc.Texture2DMSArray = {
                    .FirstArraySlice = desc.base_array_layer,
                    .ArraySize       = desc.array_layer_count,
                };
            } break;
            default: {
                // Nothing to do
            }
        }
    }

    return dsv_desc;
}

inline constexpr auto range_type_to_slot_type(D3D12_DESCRIPTOR_RANGE_TYPE type) noexcept {
    switch (type) {
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            return ResourceBinder::SlotType::SRV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            return ResourceBinder::SlotType::UAV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            return ResourceBinder::SlotType::CBV;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            return ResourceBinder::SlotType::Sampler;
    }
}

inline constexpr auto root_parameter_type_to_slot_type(D3D12_ROOT_PARAMETER_TYPE type) {
    switch (type) {
        case D3D12_ROOT_PARAMETER_TYPE_CBV:
            return ResourceBinder::SlotType::CBV;
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
            return ResourceBinder::SlotType::SRV;
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
            return ResourceBinder::SlotType::UAV;
        default:
            throw std::invalid_argument("only root descriptor type can be cast!");
    }
}

}  // namespace hitagi::gfx