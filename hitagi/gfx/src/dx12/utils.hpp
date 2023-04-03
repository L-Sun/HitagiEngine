#pragma once
#include "dx12_resource.hpp"
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/command_context.hpp>

#include <d3d12.h>
#include <comdef.h>

namespace hitagi::gfx {

inline std::string HrToString(HRESULT hr) {
    _com_error err(hr);
    return {err.ErrorMessage()};
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

inline constexpr auto to_dxgi_format(Format format) noexcept -> DXGI_FORMAT {
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

inline constexpr auto from_dxgi_format(DXGI_FORMAT format) noexcept -> Format {
    switch (format) {
        case DXGI_FORMAT_UNKNOWN:
            return Format::UNKNOWN;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return Format::R32G32B32A32_TYPELESS;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return Format::R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32A32_UINT:
            return Format::R32G32B32A32_UINT;
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return Format::R32G32B32A32_SINT;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return Format::R32G32B32_TYPELESS;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return Format::R32G32B32_FLOAT;
        case DXGI_FORMAT_R32G32B32_UINT:
            return Format::R32G32B32_UINT;
        case DXGI_FORMAT_R32G32B32_SINT:
            return Format::R32G32B32_SINT;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return Format::R16G16B16A16_TYPELESS;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return Format::R16G16B16A16_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return Format::R16G16B16A16_UNORM;
        case DXGI_FORMAT_R16G16B16A16_UINT:
            return Format::R16G16B16A16_UINT;
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            return Format::R16G16B16A16_SNORM;
        case DXGI_FORMAT_R16G16B16A16_SINT:
            return Format::R16G16B16A16_SINT;
        case DXGI_FORMAT_R32G32_TYPELESS:
            return Format::R32G32_TYPELESS;
        case DXGI_FORMAT_R32G32_FLOAT:
            return Format::R32G32_FLOAT;
        case DXGI_FORMAT_R32G32_UINT:
            return Format::R32G32_UINT;
        case DXGI_FORMAT_R32G32_SINT:
            return Format::R32G32_SINT;
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return Format::R32G8X24_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return Format::D32_FLOAT_S8X24_UINT;
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            return Format::R32_FLOAT_X8X24_TYPELESS;
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return Format::X32_TYPELESS_G8X24_UINT;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return Format::R10G10B10A2_TYPELESS;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            return Format::R10G10B10A2_UNORM;
        case DXGI_FORMAT_R10G10B10A2_UINT:
            return Format::R10G10B10A2_UINT;
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return Format::R11G11B10_FLOAT;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return Format::R8G8B8A8_TYPELESS;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return Format::R8G8B8A8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return Format::R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_R8G8B8A8_UINT:
            return Format::R8G8B8A8_UINT;
        case DXGI_FORMAT_R8G8B8A8_SNORM:
            return Format::R8G8B8A8_SNORM;
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return Format::R8G8B8A8_SINT;
        case DXGI_FORMAT_R16G16_TYPELESS:
            return Format::R16G16_TYPELESS;
        case DXGI_FORMAT_R16G16_FLOAT:
            return Format::R16G16_FLOAT;
        case DXGI_FORMAT_R16G16_UNORM:
            return Format::R16G16_UNORM;
        case DXGI_FORMAT_R16G16_UINT:
            return Format::R16G16_UINT;
        case DXGI_FORMAT_R16G16_SNORM:
            return Format::R16G16_SNORM;
        case DXGI_FORMAT_R16G16_SINT:
            return Format::R16G16_SINT;
        case DXGI_FORMAT_R32_TYPELESS:
            return Format::R32_TYPELESS;
        case DXGI_FORMAT_D32_FLOAT:
            return Format::D32_FLOAT;
        case DXGI_FORMAT_R32_FLOAT:
            return Format::R32_FLOAT;
        case DXGI_FORMAT_R32_UINT:
            return Format::R32_UINT;
        case DXGI_FORMAT_R32_SINT:
            return Format::R32_SINT;
        case DXGI_FORMAT_R24G8_TYPELESS:
            return Format::R24G8_TYPELESS;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return Format::D24_UNORM_S8_UINT;
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
            return Format::R24_UNORM_X8_TYPELESS;
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return Format::X24_TYPELESS_G8_UINT;
        case DXGI_FORMAT_R8G8_TYPELESS:
            return Format::R8G8_TYPELESS;
        case DXGI_FORMAT_R8G8_UNORM:
            return Format::R8G8_UNORM;
        case DXGI_FORMAT_R8G8_UINT:
            return Format::R8G8_UINT;
        case DXGI_FORMAT_R8G8_SNORM:
            return Format::R8G8_SNORM;
        case DXGI_FORMAT_R8G8_SINT:
            return Format::R8G8_SINT;
        case DXGI_FORMAT_R16_TYPELESS:
            return Format::R16_TYPELESS;
        case DXGI_FORMAT_R16_FLOAT:
            return Format::R16_FLOAT;
        case DXGI_FORMAT_D16_UNORM:
            return Format::D16_UNORM;
        case DXGI_FORMAT_R16_UNORM:
            return Format::R16_UNORM;
        case DXGI_FORMAT_R16_UINT:
            return Format::R16_UINT;
        case DXGI_FORMAT_R16_SNORM:
            return Format::R16_SNORM;
        case DXGI_FORMAT_R16_SINT:
            return Format::R16_SINT;
        case DXGI_FORMAT_R8_TYPELESS:
            return Format::R8_TYPELESS;
        case DXGI_FORMAT_R8_UNORM:
            return Format::R8_UNORM;
        case DXGI_FORMAT_R8_UINT:
            return Format::R8_UINT;
        case DXGI_FORMAT_R8_SNORM:
            return Format::R8_SNORM;
        case DXGI_FORMAT_R8_SINT:
            return Format::R8_SINT;
        case DXGI_FORMAT_A8_UNORM:
            return Format::A8_UNORM;
        case DXGI_FORMAT_R1_UNORM:
            return Format::R1_UNORM;
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            return Format::R9G9B9E5_SHAREDEXP;
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
            return Format::R8G8_B8G8_UNORM;
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
            return Format::G8R8_G8B8_UNORM;
        case DXGI_FORMAT_BC1_TYPELESS:
            return Format::BC1_TYPELESS;
        case DXGI_FORMAT_BC1_UNORM:
            return Format::BC1_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return Format::BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_TYPELESS:
            return Format::BC2_TYPELESS;
        case DXGI_FORMAT_BC2_UNORM:
            return Format::BC2_UNORM;
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return Format::BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_TYPELESS:
            return Format::BC3_TYPELESS;
        case DXGI_FORMAT_BC3_UNORM:
            return Format::BC3_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return Format::BC3_UNORM_SRGB;
        case DXGI_FORMAT_BC4_TYPELESS:
            return Format::BC4_TYPELESS;
        case DXGI_FORMAT_BC4_UNORM:
            return Format::BC4_UNORM;
        case DXGI_FORMAT_BC4_SNORM:
            return Format::BC4_SNORM;
        case DXGI_FORMAT_BC5_TYPELESS:
            return Format::BC5_TYPELESS;
        case DXGI_FORMAT_BC5_UNORM:
            return Format::BC5_UNORM;
        case DXGI_FORMAT_BC5_SNORM:
            return Format::BC5_SNORM;
        case DXGI_FORMAT_B5G6R5_UNORM:
            return Format::B5G6R5_UNORM;
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return Format::B5G5R5A1_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return Format::B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return Format::B8G8R8X8_UNORM;
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
            return Format::R10G10B10_XR_BIAS_A2_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            return Format::B8G8R8A8_TYPELESS;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return Format::B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            return Format::B8G8R8X8_TYPELESS;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return Format::B8G8R8X8_UNORM_SRGB;
        case DXGI_FORMAT_BC6H_TYPELESS:
            return Format::BC6H_TYPELESS;
        case DXGI_FORMAT_BC6H_UF16:
            return Format::BC6H_UF16;
        case DXGI_FORMAT_BC6H_SF16:
            return Format::BC6H_SF16;
        case DXGI_FORMAT_BC7_TYPELESS:
            return Format::BC7_TYPELESS;
        case DXGI_FORMAT_BC7_UNORM:
            return Format::BC7_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return Format::BC7_UNORM_SRGB;
        default:
            return Format::UNKNOWN;
    }
}

inline constexpr auto to_d3d_clear_value(ClearValue value, Format format) noexcept -> D3D12_CLEAR_VALUE {
    D3D12_CLEAR_VALUE clear_value;
    clear_value.Format = to_dxgi_format(format);
    if (std::holds_alternative<ClearColor>(value)) {
        clear_value.Color[0] = std::get<ClearColor>(value)[0];
        clear_value.Color[1] = std::get<ClearColor>(value)[1];
        clear_value.Color[2] = std::get<ClearColor>(value)[2];
        clear_value.Color[3] = std::get<ClearColor>(value)[3];
    } else {
        clear_value.DepthStencil.Depth   = std::get<ClearDepthStencil>(value).depth;
        clear_value.DepthStencil.Stencil = std::get<ClearDepthStencil>(value).stencil;
    }
    return clear_value;
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

inline constexpr auto to_d3d_address_mode(AddressMode mode) noexcept {
    switch (mode) {
        case AddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case AddressMode::Repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case AddressMode::MirrorRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
}

inline constexpr auto to_d3d_filter_type(FilterMode mode) noexcept {
    switch (mode) {
        case FilterMode::Point:
            return D3D12_FILTER_TYPE_POINT;
        case FilterMode::Linear:
            return D3D12_FILTER_TYPE_LINEAR;
    }
}

inline constexpr auto to_d3d_compare_function(CompareOp op) noexcept {
    switch (op) {
        case CompareOp::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case CompareOp::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case CompareOp::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareOp::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareOp::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case CompareOp::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareOp::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareOp::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

inline constexpr auto to_d3d_blend(BlendFactor blend) noexcept {
    switch (blend) {
        case BlendFactor::Zero:
            return D3D12_BLEND_ZERO;
        case BlendFactor::One:
            return D3D12_BLEND_ONE;
        case BlendFactor::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::Constant:
            return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::InvConstant:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            return D3D12_BLEND_ZERO;
    }
}

inline constexpr auto to_d3d_blend_op(BlendOp op) noexcept {
    switch (op) {
        case BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
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
        case LogicOp::NoOp:
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

inline constexpr auto to_d3d_color_mask(ColorMask mask) noexcept {
    return static_cast<D3D12_COLOR_WRITE_ENABLE>(mask);
}

inline constexpr auto to_d3d_blend_desc(BlendState state) noexcept {
    D3D12_BLEND_DESC result{};
    result.AlphaToCoverageEnable  = false;
    result.IndependentBlendEnable = false;

    result.RenderTarget[0].BlendEnable           = state.blend_enable;
    result.RenderTarget[0].LogicOpEnable         = state.logic_operation_enable;
    result.RenderTarget[0].SrcBlend              = to_d3d_blend(state.src_color_blend_factor);
    result.RenderTarget[0].DestBlend             = to_d3d_blend(state.dst_color_blend_factor);
    result.RenderTarget[0].BlendOp               = to_d3d_blend_op(state.color_blend_op);
    result.RenderTarget[0].SrcBlendAlpha         = to_d3d_blend(state.src_alpha_blend_factor);
    result.RenderTarget[0].DestBlendAlpha        = to_d3d_blend(state.dst_alpha_blend_factor);
    result.RenderTarget[0].BlendOpAlpha          = to_d3d_blend_op(state.alpha_blend_op);
    result.RenderTarget[0].LogicOp               = to_d3d_logic_op(state.logic_op);
    result.RenderTarget[0].RenderTargetWriteMask = to_d3d_color_mask(state.color_write_mask);

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

// for holding object life time
struct DX12InputLayout {
    std::pmr::vector<D3D12_INPUT_ELEMENT_DESC> attributes;
    D3D12_INPUT_LAYOUT_DESC                    desc;
};

inline constexpr auto to_d3d_input_layout(const VertexLayout& layout) noexcept {
    DX12InputLayout result;
    for (const auto& attribute : layout) {
        result.attributes.emplace_back(D3D12_INPUT_ELEMENT_DESC{
            .SemanticName         = attribute.semantic_name.data(),
            .SemanticIndex        = attribute.semantic_index,
            .Format               = to_dxgi_format(attribute.format),
            .InputSlot            = attribute.binding,
            .AlignedByteOffset    = static_cast<UINT>(attribute.offset),
            .InputSlotClass       = attribute.per_instance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = attribute.per_instance ? 1u : 0u,
        });
    }
    result.desc.pInputElementDescs = result.attributes.data();
    result.desc.NumElements        = result.attributes.size();

    return result;
}

inline constexpr auto to_d3d_rasterizer_state(RasterizationState state) noexcept {
    D3D12_RASTERIZER_DESC result = {
        .FillMode              = to_d3d_fill_mode(state.fill_mode),
        .CullMode              = to_d3d_cull_mode(state.cull_mode),
        .FrontCounterClockwise = state.front_counter_clockwise,
        .DepthBias             = static_cast<int>(state.depth_bias),
        .DepthBiasClamp        = state.depth_bias_clamp,
        .SlopeScaledDepthBias  = state.depth_bias_slope_factor,
        .DepthClipEnable       = state.depth_clamp_enable,
        .MultisampleEnable     = false,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount     = 0,
        .ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    };

    return result;
}

inline constexpr auto to_d3d_stencil_op(StencilOp op) noexcept {
    switch (op) {
        case StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::IncrementClamp:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrementClamp:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::IncrementWrap:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::DecrementWrap:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_KEEP;
    }
}

inline constexpr auto to_d3d_stencil_op_state(StencilOpState state) noexcept {
    return D3D12_DEPTH_STENCILOP_DESC{
        .StencilFailOp      = to_d3d_stencil_op(state.fail_op),
        .StencilDepthFailOp = to_d3d_stencil_op(state.depth_fail_op),
        .StencilPassOp      = to_d3d_stencil_op(state.pass_op),
        .StencilFunc        = to_d3d_compare_function(state.compare_op),
    };
}

inline constexpr auto to_d3d_depth_stencil_state(DepthStencilState state) noexcept {
    return D3D12_DEPTH_STENCIL_DESC{
        .DepthEnable    = state.depth_test_enable,
        .DepthWriteMask = state.depth_write_enable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc      = to_d3d_compare_function(state.depth_compare_op),
        .StencilEnable  = state.stencil_test_enable,
        .FrontFace      = to_d3d_stencil_op_state(state.front),
        .BackFace       = to_d3d_stencil_op_state(state.back),
        // TODO
        // .StencilReadMask  = state.front.compare_mask,
        // .DepthWriteMask   = state.front.write_mask,
        // .StencilWriteMask = state.front.write_mask,
    };
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

inline constexpr auto to_d3d_srv_desc(const TextureDesc& desc) noexcept {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format                  = to_dxgi_format(desc.format),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    // Dimension detect
    {
        if (desc.height == 1 && desc.depth == 1) {
            if (desc.array_size == 1) {
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            } else {
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            }
        } else if (desc.depth == 1) {
            if (desc.array_size == 1) {
                if (utils::has_flag(desc.usages, TextureUsageFlags::Cube)) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                } else {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                }
            } else {
                if (utils::has_flag(desc.usages, TextureUsageFlags::CubeArray)) {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
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
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
                srv_desc.Texture1DArray = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .FirstArraySlice     = 0,
                    .ArraySize           = desc.array_size,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2D: {
                srv_desc.Texture2D = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .PlaneSlice          = 0,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
                srv_desc.Texture2DArray = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .FirstArraySlice     = 0,
                    .ArraySize           = desc.array_size,
                    .PlaneSlice          = 0,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
                srv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURE3D: {
                srv_desc.Texture3D = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURECUBE: {
                srv_desc.TextureCube = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
                srv_desc.TextureCubeArray = {
                    .MostDetailedMip     = 0,
                    .MipLevels           = desc.mip_levels,
                    .First2DArrayFace    = 0,
                    .NumCubes            = desc.array_size,
                    .ResourceMinLODClamp = 0.0f,
                };
            } break;
            default: {
                // TODO ray tracing
            }
        }
    }

    return srv_desc;
}

inline constexpr auto to_d3d_uav_desc(const TextureDesc& desc) noexcept {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.height == 1 && desc.depth == 1) {
            if (desc.array_size == 1) {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            } else {
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            }
        } else if (desc.depth == 1) {
            if (desc.array_size == 1) {
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
                    .MipSlice = 0,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
                uav_desc.Texture1DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE2D: {
                uav_desc.Texture2D = {
                    .MipSlice   = 0,
                    .PlaneSlice = 0,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
                uav_desc.Texture2DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                    .PlaneSlice      = 0,
                };
            } break;
            case D3D12_UAV_DIMENSION_TEXTURE3D: {
                uav_desc.Texture3D = {
                    .MipSlice    = 0,
                    .FirstWSlice = 0,
                    .WSize       = desc.depth,
                };
            } break;
            default: {
            }
        }
    }

    return uav_desc;
}

inline auto to_d3d_rtv_desc(const TextureDesc& desc) noexcept {
    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.height == 1 && desc.depth == 1) {
            if (desc.array_size == 1) {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
            } else {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            }

        } else if (desc.depth == 1) {
            if (desc.array_size == 1) {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            } else {
                rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
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
                    .MipSlice = 0,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: {
                rtv_desc.Texture1DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2D: {
                rtv_desc.Texture2D = {
                    .MipSlice   = 0,
                    .PlaneSlice = 0,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: {
                rtv_desc.Texture2DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                    .PlaneSlice      = 0,
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE2DMS: {
                rtv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            case D3D12_RTV_DIMENSION_TEXTURE3D: {
                rtv_desc.Texture3D = {
                    .MipSlice = 0,
                };
            } break;
            default: {
                // Nothing to do
            }
        }
    }

    return rtv_desc;
}

inline auto to_d3d_dsv_desc(const TextureDesc& desc) noexcept {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{
        .Format = to_dxgi_format(desc.format),
    };

    // Dimension detect
    {
        if (desc.height == 1 && desc.depth == 1) {
            if (desc.array_size == 1) {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
            } else {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
            }

        } else if (desc.depth == 1) {
            if (desc.array_size == 1) {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            } else {
                dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
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
                    .MipSlice = 0,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: {
                dsv_desc.Texture1DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2D: {
                dsv_desc.Texture2D = {
                    .MipSlice = 0,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: {
                dsv_desc.Texture2DArray = {
                    .MipSlice        = 0,
                    .FirstArraySlice = 0,
                    .ArraySize       = desc.array_size,
                };
            } break;
            case D3D12_DSV_DIMENSION_TEXTURE2DMS: {
                dsv_desc.Texture2DMS = {
                    // UnusedField_NothingToDefine
                };
            } break;
            default: {
                // Nothing to do
            }
        }
    }

    return dsv_desc;
}

inline Format get_format(D3D_REGISTER_COMPONENT_TYPE type, BYTE mask) {
    std::size_t num_components = std::countr_one(mask);
    switch (num_components) {
        case 1: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return Format::UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return Format::R32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return Format::R32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return Format::R32_FLOAT;
            }
        }
        case 2: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return Format::UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return Format::R32G32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return Format::R32G32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return Format::R32G32_FLOAT;
            }
        }
        case 3: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return Format::UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return Format::R32G32B32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return Format::R32G32B32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return Format::R32G32B32_FLOAT;
            }
        }
        case 4: {
            switch (type) {
                case D3D_REGISTER_COMPONENT_UNKNOWN:
                    return Format::UNKNOWN;
                case D3D_REGISTER_COMPONENT_UINT32:
                    return Format::R32G32B32A32_UINT;
                case D3D_REGISTER_COMPONENT_SINT32:
                    return Format::R32G32B32A32_SINT;
                case D3D_REGISTER_COMPONENT_FLOAT32:
                    return Format::R32G32B32A32_FLOAT;
            }
        }
        default:
            return Format::UNKNOWN;
    }
}

inline constexpr auto to_d3d_barrier_access(BarrierAccess access) noexcept -> D3D12_BARRIER_ACCESS {
    D3D12_BARRIER_ACCESS d3d_access = D3D12_BARRIER_ACCESS_COMMON;
    if (access == BarrierAccess::Unkown) {
        return D3D12_BARRIER_ACCESS_NO_ACCESS;
    }
    if (utils::has_flag(access, BarrierAccess::CopySrc)) {
        d3d_access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
    }
    if (utils::has_flag(access, BarrierAccess::CopyDst)) {
        d3d_access |= D3D12_BARRIER_ACCESS_COPY_DEST;
    }
    if (utils::has_flag(access, BarrierAccess::Vertex)) {
        d3d_access |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
    }
    if (utils::has_flag(access, BarrierAccess::Index)) {
        d3d_access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
    }
    if (utils::has_flag(access, BarrierAccess::Constant)) {
        d3d_access |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
    }
    if (utils::has_flag(access, BarrierAccess::ShaderRead)) {
        d3d_access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    }
    if (utils::has_flag(access, BarrierAccess::ShaderWrite)) {
        d3d_access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    }
    if (utils::has_flag(access, BarrierAccess::RenderTarget)) {
        d3d_access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
    }
    if (utils::has_flag(access, BarrierAccess::Present)) {
        d3d_access |= D3D12_BARRIER_ACCESS_COMMON;
    }

    return d3d_access;
}

inline constexpr auto to_d3d_pipeline_stage(PipelineStage stage) noexcept -> D3D12_BARRIER_SYNC {
    D3D12_BARRIER_SYNC d3d_stage = D3D12_BARRIER_SYNC_NONE;
    if (stage == PipelineStage::None) {
        return D3D12_BARRIER_SYNC_NONE;
    }
    if (utils::has_flag(stage, PipelineStage::VertexInput)) {
        d3d_stage |= D3D12_BARRIER_SYNC_INDEX_INPUT;
    }
    if (utils::has_flag(stage, PipelineStage::VertexShader)) {
        d3d_stage |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
    }
    if (utils::has_flag(stage, PipelineStage::PixelShader)) {
        d3d_stage |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
    }
    if (utils::has_flag(stage, PipelineStage::DepthStencil)) {
        d3d_stage |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    }
    if (utils::has_flag(stage, PipelineStage::Render)) {
        d3d_stage |= D3D12_BARRIER_SYNC_RENDER_TARGET;
    }
    if (utils::has_flag(stage, PipelineStage::ComputeShader)) {
        d3d_stage |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    }
    if (utils::has_flag(stage, PipelineStage::AllGraphics)) {
        d3d_stage |= D3D12_BARRIER_SYNC_ALL_SHADING;
    }
    if (utils::has_flag(stage, PipelineStage::All)) {
        d3d_stage |= D3D12_BARRIER_SYNC_ALL;
    }
    return d3d_stage;
}

inline constexpr auto to_d3d_texture_layout(TextureLayout layout) noexcept -> D3D12_BARRIER_LAYOUT {
    switch (layout) {
        case TextureLayout::Unkown:
            return D3D12_BARRIER_LAYOUT_UNDEFINED;
        case TextureLayout::Common:
            return D3D12_BARRIER_LAYOUT_COMMON;
        case TextureLayout::CopySrc:
            return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        case TextureLayout::CopyDst:
            return D3D12_BARRIER_LAYOUT_COPY_DEST;
        case TextureLayout::ShaderRead:
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        case TextureLayout::ShaderWrite:
            return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        case TextureLayout::DepthStencilRead:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        case TextureLayout::DepthStencilWrite:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        case TextureLayout::RenderTarget:
            return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        case TextureLayout::ResolveSrc:
            return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        case TextureLayout::ResolveDst:
            return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        case TextureLayout::Present:
            return D3D12_BARRIER_LAYOUT_PRESENT;
    }
}

inline constexpr auto to_d3d_global_barrier(GlobalBarrier barrier) noexcept -> D3D12_GLOBAL_BARRIER {
    return {
        .SyncBefore   = to_d3d_pipeline_stage(barrier.src_stage),
        .SyncAfter    = to_d3d_pipeline_stage(barrier.dst_stage),
        .AccessBefore = to_d3d_barrier_access(barrier.src_access),
        .AccessAfter  = to_d3d_barrier_access(barrier.dst_access),
    };
}

inline constexpr auto to_d3d_buffer_barrier(GPUBufferBarrier barrier) noexcept -> D3D12_BUFFER_BARRIER {
    return {
        .SyncBefore   = to_d3d_pipeline_stage(barrier.src_stage),
        .SyncAfter    = to_d3d_pipeline_stage(barrier.dst_stage),
        .AccessBefore = to_d3d_barrier_access(barrier.src_access),
        .AccessAfter  = to_d3d_barrier_access(barrier.dst_access),
        .pResource    = static_cast<DX12GPUBuffer&>(barrier.buffer).resource.Get(),
        .Offset       = 0,
        .Size         = static_cast<DX12GPUBuffer&>(barrier.buffer).buffer_size,
    };
}

inline constexpr auto to_d3d_texture_barrier(TextureBarrier barrier) noexcept -> D3D12_TEXTURE_BARRIER {
    return {
        .SyncBefore   = to_d3d_pipeline_stage(barrier.src_stage),
        .SyncAfter    = to_d3d_pipeline_stage(barrier.dst_stage),
        .AccessBefore = to_d3d_barrier_access(barrier.src_access),
        .AccessAfter  = to_d3d_barrier_access(barrier.dst_access),
        .LayoutBefore = to_d3d_texture_layout(barrier.src_layout),
        .LayoutAfter  = to_d3d_texture_layout(barrier.dst_layout),
        .pResource    = static_cast<DX12Texture&>(barrier.texture).resource.Get(),
        .Subresources = {
            .IndexOrFirstMipLevel = 0,
            .NumMipLevels         = barrier.texture.GetDesc().mip_levels,
            .FirstArraySlice      = 0,
            .NumArraySlices       = barrier.texture.GetDesc().array_size,
            .FirstPlane           = 0,
            .NumPlanes            = 1,
        },
        .Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE,
    };
};

}  // namespace hitagi::gfx