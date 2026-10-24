export module gfx.metal:utils;
import interop.metal;
import interop.spdlog;

import std;
import gfx.base;

export namespace hitagi::gfx {

namespace mtl  = interop::mtl;
namespace mtl4 = interop::mtl4;
namespace ns   = interop::ns;
namespace ca   = interop::ca;

auto to_metal_string(std::string_view value) -> ns::String*;
void check_metal(const std::shared_ptr<spdlog::logger>& logger, bool success, std::string_view message, ns::Error* error = nullptr);
auto to_metal_pixel_format(Format format) -> mtl::PixelFormat;
auto to_metal_vertex_format(Format format) -> mtl::VertexFormat;
auto to_metal_compare_function(CompareOp op) -> mtl::CompareFunction;
auto to_metal_blend_factor(BlendFactor factor) -> mtl::BlendFactor;
auto to_metal_blend_operation(BlendOp op) -> mtl::BlendOperation;

}  // namespace hitagi::gfx

namespace hitagi::gfx {

auto to_metal_string(std::string_view value) -> ns::String* { return ns::String::string(std::pmr::string(value).c_str(), ns::UTF8StringEncoding); }

void check_metal(const std::shared_ptr<spdlog::logger>& logger, bool success, std::string_view message, ns::Error* error) {
    if (success) return;
    std::pmr::string error_message(message);
    if (error) {
        error_message += ": ";
        error_message += error->localizedDescription()->utf8String();
    }
    logger->error("{}", error_message);
    throw std::runtime_error(error_message.c_str());
}

auto to_metal_pixel_format(Format format) -> mtl::PixelFormat {
    switch (format) {
        case Format::R8G8B8A8_UNORM:
            return mtl::PixelFormatRGBA8Unorm;
        case Format::R8G8B8A8_UNORM_SRGB:
            return mtl::PixelFormatRGBA8Unorm_sRGB;
        case Format::B8G8R8A8_UNORM:
            return mtl::PixelFormatBGRA8Unorm;
        case Format::B8G8R8A8_UNORM_SRGB:
            return mtl::PixelFormatBGRA8Unorm_sRGB;
        case Format::R11G11B10_FLOAT:
            return mtl::PixelFormatRG11B10Float;
        case Format::R16G16B16A16_FLOAT:
            return mtl::PixelFormatRGBA16Float;
        case Format::R32G32B32A32_FLOAT:
            return mtl::PixelFormatRGBA32Float;
        case Format::R32G32_FLOAT:
            return mtl::PixelFormatRG32Float;
        case Format::R16G16_FLOAT:
            return mtl::PixelFormatRG16Float;
        case Format::R32_FLOAT:
            return mtl::PixelFormatR32Float;
        case Format::R32_UINT:
            return mtl::PixelFormatR32Uint;
        case Format::R32_SINT:
            return mtl::PixelFormatR32Sint;
        case Format::R16_FLOAT:
            return mtl::PixelFormatR16Float;
        case Format::R8_UNORM:
            return mtl::PixelFormatR8Unorm;
        case Format::R8_UINT:
            return mtl::PixelFormatR8Uint;
        case Format::D32_FLOAT:
            return mtl::PixelFormatDepth32Float;
        case Format::D32_FLOAT_S8X24_UINT:
            return mtl::PixelFormatDepth32Float_Stencil8;
        case Format::R32G32_UINT:
            return mtl::PixelFormatRG32Uint;
        case Format::R32G32_SINT:
            return mtl::PixelFormatRG32Sint;
        case Format::R32G32B32A32_UINT:
            return mtl::PixelFormatRGBA32Uint;
        case Format::R32G32B32A32_SINT:
            return mtl::PixelFormatRGBA32Sint;
        case Format::R16G16B16A16_UNORM:
            return mtl::PixelFormatRGBA16Unorm;
        case Format::R16G16B16A16_SNORM:
            return mtl::PixelFormatRGBA16Snorm;
        case Format::R16G16B16A16_UINT:
            return mtl::PixelFormatRGBA16Uint;
        case Format::R16G16B16A16_SINT:
            return mtl::PixelFormatRGBA16Sint;
        case Format::R16G16_UNORM:
            return mtl::PixelFormatRG16Unorm;
        case Format::R16G16_SNORM:
            return mtl::PixelFormatRG16Snorm;
        case Format::R16G16_UINT:
            return mtl::PixelFormatRG16Uint;
        case Format::R16G16_SINT:
            return mtl::PixelFormatRG16Sint;
        case Format::R16_UNORM:
            return mtl::PixelFormatR16Unorm;
        case Format::R16_SNORM:
            return mtl::PixelFormatR16Snorm;
        case Format::R16_UINT:
            return mtl::PixelFormatR16Uint;
        case Format::R16_SINT:
            return mtl::PixelFormatR16Sint;
        case Format::R8G8_UNORM:
            return mtl::PixelFormatRG8Unorm;
        case Format::R8G8_SNORM:
            return mtl::PixelFormatRG8Snorm;
        case Format::R8G8_UINT:
            return mtl::PixelFormatRG8Uint;
        case Format::R8G8_SINT:
            return mtl::PixelFormatRG8Sint;
        case Format::R8_SNORM:
            return mtl::PixelFormatR8Snorm;
        case Format::R8_SINT:
            return mtl::PixelFormatR8Sint;
        case Format::R8G8B8A8_SNORM:
            return mtl::PixelFormatRGBA8Snorm;
        case Format::R8G8B8A8_UINT:
            return mtl::PixelFormatRGBA8Uint;
        case Format::R8G8B8A8_SINT:
            return mtl::PixelFormatRGBA8Sint;
        case Format::R10G10B10A2_UNORM:
            return mtl::PixelFormatRGB10A2Unorm;
        case Format::R10G10B10A2_UINT:
            return mtl::PixelFormatRGB10A2Uint;
        case Format::D16_UNORM:
            return mtl::PixelFormatDepth16Unorm;
        default:
            throw std::invalid_argument("Unsupported Metal pixel format " + std::to_string(static_cast<unsigned>(format)));
    }
}

auto to_metal_vertex_format(Format format) -> mtl::VertexFormat {
    switch (format) {
        case Format::R32_FLOAT:
            return mtl::VertexFormatFloat;
        case Format::R32G32_FLOAT:
            return mtl::VertexFormatFloat2;
        case Format::R32G32B32_FLOAT:
            return mtl::VertexFormatFloat3;
        case Format::R32G32B32A32_FLOAT:
            return mtl::VertexFormatFloat4;
        case Format::R32_UINT:
            return mtl::VertexFormatUInt;
        case Format::R32G32_UINT:
            return mtl::VertexFormatUInt2;
        case Format::R32G32B32A32_UINT:
            return mtl::VertexFormatUInt4;
        case Format::R8G8B8A8_UNORM:
            return mtl::VertexFormatUChar4Normalized;
        default:
            throw std::invalid_argument("Unsupported Metal vertex format");
    }
}

auto to_metal_compare_function(CompareOp op) -> mtl::CompareFunction {
    switch (op) {
        case CompareOp::Never:
            return mtl::CompareFunctionNever;
        case CompareOp::Less:
            return mtl::CompareFunctionLess;
        case CompareOp::Equal:
            return mtl::CompareFunctionEqual;
        case CompareOp::LessEqual:
            return mtl::CompareFunctionLessEqual;
        case CompareOp::Greater:
            return mtl::CompareFunctionGreater;
        case CompareOp::NotEqual:
            return mtl::CompareFunctionNotEqual;
        case CompareOp::GreaterEqual:
            return mtl::CompareFunctionGreaterEqual;
        case CompareOp::Always:
            return mtl::CompareFunctionAlways;
    }
    throw std::invalid_argument("Invalid Metal depth comparison");
}

auto to_metal_blend_factor(BlendFactor factor) -> mtl::BlendFactor {
    switch (factor) {
        case BlendFactor::Zero:
            return mtl::BlendFactorZero;
        case BlendFactor::One:
            return mtl::BlendFactorOne;
        case BlendFactor::SrcColor:
            return mtl::BlendFactorSourceColor;
        case BlendFactor::InvSrcColor:
            return mtl::BlendFactorOneMinusSourceColor;
        case BlendFactor::SrcAlpha:
            return mtl::BlendFactorSourceAlpha;
        case BlendFactor::InvSrcAlpha:
            return mtl::BlendFactorOneMinusSourceAlpha;
        case BlendFactor::DstColor:
            return mtl::BlendFactorDestinationColor;
        case BlendFactor::InvDstColor:
            return mtl::BlendFactorOneMinusDestinationColor;
        case BlendFactor::DstAlpha:
            return mtl::BlendFactorDestinationAlpha;
        case BlendFactor::InvDstAlpha:
            return mtl::BlendFactorOneMinusDestinationAlpha;
        case BlendFactor::Constant:
            return mtl::BlendFactorBlendColor;
        case BlendFactor::InvConstant:
            return mtl::BlendFactorOneMinusBlendColor;
        case BlendFactor::SrcAlphaSat:
            return mtl::BlendFactorSourceAlphaSaturated;
    }
    throw std::invalid_argument("Invalid Metal blend factor");
}

auto to_metal_blend_operation(BlendOp op) -> mtl::BlendOperation {
    switch (op) {
        case BlendOp::Add:
            return mtl::BlendOperationAdd;
        case BlendOp::Subtract:
            return mtl::BlendOperationSubtract;
        case BlendOp::ReverseSubtract:
            return mtl::BlendOperationReverseSubtract;
        case BlendOp::Min:
            return mtl::BlendOperationMin;
        case BlendOp::Max:
            return mtl::BlendOperationMax;
    }
    throw std::invalid_argument("Invalid Metal blend operation");
}

}  // namespace hitagi::gfx
