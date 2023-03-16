#pragma once
#include "vk_configs.hpp"
#include "vk_sync.hpp"
#include "vk_resource.hpp"

#include <hitagi/gfx/command_context.hpp>
#include <hitagi/utils/array.hpp>
#include <hitagi/utils/soa.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <SDL2/SDL_vulkan.h>

#include <set>

namespace hitagi::gfx {

auto custom_vk_allocation_fn(void* p_this, std::size_t size, std::size_t alignment, VkSystemAllocationScope) -> void*;
auto custom_vk_reallocation_fn(void* p_this, void* origin_ptr, std::size_t new_size, std::size_t alignment, VkSystemAllocationScope) -> void*;
auto custom_vk_free_fn(void* p_this, void* ptr) -> void;
auto custom_debug_message_fn(VkDebugUtilsMessageSeverityFlagBitsEXT _severity, VkDebugUtilsMessageTypeFlagsEXT _type, VkDebugUtilsMessengerCallbackDataEXT const* p_data, void* p_logger) -> VkBool32;

inline auto compute_physical_device_score(const vk::raii::PhysicalDevice& device) noexcept {
    int score = 0;

    const auto device_properties = device.getProperties();
    const auto device_features   = device.getFeatures();

    if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    score += device_properties.limits.maxImageDimension2D;

    if (!device_features.geometryShader) {
        score = 0;
    }

    return score;
}

inline auto is_physical_suitable(const vk::raii::PhysicalDevice& physical_device) -> bool {
    const auto queue_family_properties = physical_device.getQueueFamilyProperties();

    bool queue_family_found = false;
    for (const auto& queue_family_properties : queue_family_properties) {
        if (queue_family_properties.queueCount >= magic_enum::enum_count<CommandType>() &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eCompute) &&
            (queue_family_properties.queueFlags & vk::QueueFlagBits::eTransfer)) {
            queue_family_found = true;
            break;
        }
    }

    const auto supported_extensions = physical_device.enumerateDeviceExtensionProperties();

    std::pmr::set<std::pmr::string> required_extensions = {required_device_extensions.begin(), required_device_extensions.end()};

    for (const auto& extension : supported_extensions) {
        required_extensions.erase(std::pmr::string{extension.extensionName});
    }
    return queue_family_found && required_extensions.empty();
}

inline auto get_sdl2_drawable_size(SDL_Window* window) -> math::vec2u {
    int width, height;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    return {width, height};
}

inline constexpr auto get_command_label_color(CommandType type) {
    constexpr std::array graphics_color = {0.48f, 0.76f, 0.47f, 1.0f};
    constexpr std::array compute_color  = {0.38f, 0.69f, 0.94f, 1.0f};
    constexpr std::array copy_color     = {0.88f, 0.39f, 0.35f, 1.0f};

    switch (type) {
        case CommandType::Graphics:
            return graphics_color;
        case CommandType::Compute:
            return compute_color;
        case CommandType::Copy:
            return copy_color;
    };
}

template <typename T>
inline void create_vk_debug_object_info(const T& obj, std::string_view name, const vk::raii::Device& vk_device) {
    vk_device.setDebugUtilsObjectNameEXT({
        .objectType   = obj.objectType,
        .objectHandle = reinterpret_cast<std::uintptr_t>(static_cast<typename T::CType>(*obj)),
        .pObjectName  = name.data(),
    });
}

inline auto convert_to_sao_vk_semaphore_wait_pairs(const std::pmr::vector<SemaphoreWaitPair>& pairs) {
    utils::SoA<vk::Semaphore, std::uint64_t> result;
    std::transform(pairs.begin(), pairs.end(), std::back_inserter(result), [](const SemaphoreWaitPair& pair) {
        auto&& [semaphore, value] = pair;
        auto& vk_semaphore        = std::static_pointer_cast<VulkanSemaphore>(semaphore)->semaphore;
        return std::make_pair(*vk_semaphore, value);
    });

    return result;
}

inline auto get_queue_create_info(const vk::raii::PhysicalDevice& device) -> std::optional<vk::DeviceQueueCreateInfo> {
    const auto queue_family_properties = device.getQueueFamilyProperties();

    constexpr static auto priorities = utils::create_enum_array<float, CommandType>(1.0f);

    for (std::uint32_t family_index = 0; family_index < queue_family_properties.size(); family_index++) {
        if (queue_family_properties[family_index].queueCount >= magic_enum::enum_count<CommandType>() &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eGraphics) &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eCompute) &&
            (queue_family_properties[family_index].queueFlags & vk::QueueFlagBits::eTransfer)) {
            return vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = family_index,
                .queueCount       = magic_enum::enum_count<CommandType>(),
                .pQueuePriorities = priorities.data(),
            };
        };
    }
    return std::nullopt;
}

inline constexpr auto get_shader_model_version(Shader::Type type) noexcept {
    switch (type) {
        case Shader::Type::Vertex:
            return L"vs_6_7";
        case Shader::Type::Pixel:
            return L"ps_6_7";
        case Shader::Type::Geometry:
            return L"gs_6_7";
        case Shader::Type::Compute:
            return L"cs_6_7";
    }
}

inline constexpr auto to_vk_format(Format format) noexcept -> vk::Format {
    switch (format) {
        case Format::UNKNOWN:
            return vk::Format::eUndefined;
        case Format::R32G32B32A32_FLOAT:
            return vk::Format::eR32G32B32A32Sfloat;
        case Format::R32G32B32A32_UINT:
            return vk::Format::eR32G32B32A32Uint;
        case Format::R32G32B32A32_SINT:
            return vk::Format::eR32G32B32A32Sint;
        case Format::R32G32B32_FLOAT:
            return vk::Format::eR32G32B32Sfloat;
        case Format::R32G32B32_UINT:
            return vk::Format::eR32G32B32Uint;
        case Format::R32G32B32_SINT:
            return vk::Format::eR32G32B32Sint;
        case Format::R16G16B16A16_FLOAT:
            return vk::Format::eR16G16B16A16Sfloat;
        case Format::R16G16B16A16_UNORM:
            return vk::Format::eR16G16B16A16Unorm;
        case Format::R16G16B16A16_UINT:
            return vk::Format::eR16G16B16A16Uint;
        case Format::R16G16B16A16_SNORM:
            return vk::Format::eR16G16B16A16Snorm;
        case Format::R16G16B16A16_SINT:
            return vk::Format::eR16G16B16A16Sint;
        case Format::R32G32_FLOAT:
            return vk::Format::eR32G32Sfloat;
        case Format::R32G32_UINT:
            return vk::Format::eR32G32Uint;
        case Format::R32G32_SINT:
            return vk::Format::eR32G32Sint;
        case Format::D32_FLOAT_S8X24_UINT:
            return vk::Format::eD32SfloatS8Uint;
        case Format::R11G11B10_FLOAT:
            return vk::Format::eB10G11R11UfloatPack32;
        case Format::R8G8B8A8_UNORM:
            return vk::Format::eR8G8B8A8Unorm;
        case Format::R8G8B8A8_UNORM_SRGB:
            return vk::Format::eR8G8B8A8Srgb;
        case Format::R8G8B8A8_UINT:
            return vk::Format::eR8G8B8A8Uint;
        case Format::R8G8B8A8_SNORM:
            return vk::Format::eR8G8B8A8Snorm;
        case Format::R8G8B8A8_SINT:
            return vk::Format::eR8G8B8A8Sint;
        case Format::R16G16_FLOAT:
            return vk::Format::eR16G16Sfloat;
        case Format::R16G16_UNORM:
            return vk::Format::eR16G16Unorm;
        case Format::R16G16_UINT:
            return vk::Format::eR16G16Uint;
        case Format::R16G16_SNORM:
            return vk::Format::eR16G16Snorm;
        case Format::R16G16_SINT:
            return vk::Format::eR16G16Sint;
        case Format::D32_FLOAT:
            return vk::Format::eD32Sfloat;
        case Format::R32_FLOAT:
            return vk::Format::eR32Sfloat;
        case Format::R32_UINT:
            return vk::Format::eR32Uint;
        case Format::R32_SINT:
            return vk::Format::eR32Sint;
        case Format::D24_UNORM_S8_UINT:
            return vk::Format::eD24UnormS8Uint;
        case Format::R8G8_UNORM:
            return vk::Format::eR8G8Unorm;
        case Format::R8G8_UINT:
            return vk::Format::eR8G8Uint;
        case Format::R8G8_SNORM:
            return vk::Format::eR8G8Snorm;
        case Format::R8G8_SINT:
            return vk::Format::eR8G8Sint;
        case Format::R16_FLOAT:
            return vk::Format::eR16Sfloat;
        case Format::D16_UNORM:
            return vk::Format::eD16Unorm;
        case Format::R16_UNORM:
            return vk::Format::eR16Unorm;
        case Format::R16_UINT:
            return vk::Format::eR16Uint;
        case Format::R16_SNORM:
            return vk::Format::eR16Snorm;
        case Format::R16_SINT:
            return vk::Format::eR16Sint;
        case Format::R8_UNORM:
            return vk::Format::eR8Unorm;
        case Format::R8_UINT:
            return vk::Format::eR8Uint;
        case Format::R8_SNORM:
            return vk::Format::eR8Snorm;
        case Format::R8_SINT:
            return vk::Format::eR8Sint;
        case Format::B5G6R5_UNORM:
            return vk::Format::eB5G6R5UnormPack16;
        case Format::B5G5R5A1_UNORM:
            return vk::Format::eB5G5R5A1UnormPack16;
        case Format::B8G8R8A8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case Format::B8G8R8X8_UNORM:
            return vk::Format::eB8G8R8A8Unorm;
        case Format::B8G8R8A8_UNORM_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case Format::B8G8R8X8_UNORM_SRGB:
            return vk::Format::eB8G8R8A8Srgb;
        case Format::BC6H_UF16:
            return vk::Format::eBc6HUfloatBlock;
        case Format::BC6H_SF16:
            return vk::Format::eBc6HSfloatBlock;
        case Format::BC7_UNORM:
            return vk::Format::eBc7UnormBlock;
        case Format::BC7_UNORM_SRGB:
            return vk::Format::eBc7SrgbBlock;
        default:
            return vk::Format::eUndefined;
    }
}

inline constexpr auto to_vk_primitive_topology(PrimitiveTopology primitive) noexcept -> vk::PrimitiveTopology {
    switch (primitive) {
        case PrimitiveTopology::PointList:
            return vk::PrimitiveTopology::ePointList;
        case PrimitiveTopology::LineList:
            return vk::PrimitiveTopology::eLineList;
        case PrimitiveTopology::LineStrip:
            return vk::PrimitiveTopology::eLineStrip;
        case PrimitiveTopology::TriangleList:
            return vk::PrimitiveTopology::eTriangleList;
        case PrimitiveTopology::TriangleStrip:
            return vk::PrimitiveTopology::eTriangleStrip;
        default:
            return vk::PrimitiveTopology::ePointList;
    }
}

inline constexpr auto to_vk_polygon_mode(FillMode mode) noexcept -> vk::PolygonMode {
    switch (mode) {
        case FillMode::Solid:
            return vk::PolygonMode::eFill;
        case FillMode::Wireframe:
            return vk::PolygonMode::eLine;
        default:
            return vk::PolygonMode::eFill;
    }
}

inline constexpr auto to_vk_cull_mode(CullMode mode) noexcept -> vk::CullModeFlagBits {
    switch (mode) {
        case CullMode::None:
            return vk::CullModeFlagBits::eNone;
        case CullMode::Front:
            return vk::CullModeFlagBits::eFront;
        case CullMode::Back:
            return vk::CullModeFlagBits::eBack;
        default:
            return vk::CullModeFlagBits::eNone;
    }
}

inline constexpr auto to_vk_blend_factor(BlendFactor factor) noexcept -> vk::BlendFactor {
    switch (factor) {
        case BlendFactor::Zero:
            return vk::BlendFactor::eZero;
        case BlendFactor::One:
            return vk::BlendFactor::eOne;
        case BlendFactor::SrcColor:
            return vk::BlendFactor::eSrcColor;
        case BlendFactor::InvSrcColor:
            return vk::BlendFactor::eOneMinusSrcColor;
        case BlendFactor::SrcAlpha:
            return vk::BlendFactor::eSrc1Alpha;
        case BlendFactor::InvSrcAlpha:
            return vk::BlendFactor::eOneMinusSrc1Alpha;
        case BlendFactor::DstColor:
            return vk::BlendFactor::eDstColor;
        case BlendFactor::InvDstColor:
            return vk::BlendFactor::eOneMinusDstColor;
        case BlendFactor::DstAlpha:
            return vk::BlendFactor::eDstAlpha;
        case BlendFactor::InvDstAlpha:
            return vk::BlendFactor::eOneMinusDstAlpha;
        case BlendFactor::Constant:
            return vk::BlendFactor::eConstantColor;
        case BlendFactor::InvConstant:
            return vk::BlendFactor::eOneMinusConstantColor;
        case BlendFactor::SrcAlphaSat:
            return vk::BlendFactor::eSrcAlphaSaturate;
        default:
            return vk::BlendFactor::eZero;
    }
}

inline constexpr auto to_vk_color_mask(ColorMask mask) noexcept -> vk::ColorComponentFlags {
    switch (mask) {
        case ColorMask::R:
            return vk::ColorComponentFlagBits::eR;
        case ColorMask::G:
            return vk::ColorComponentFlagBits::eG;
        case ColorMask::B:
            return vk::ColorComponentFlagBits::eB;
        case ColorMask::A:
            return vk::ColorComponentFlagBits::eA;
        default:
            return vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }
}

inline constexpr auto to_vk_blend_op(BlendOp op) noexcept -> vk::BlendOp {
    switch (op) {
        case BlendOp::Add:
            return vk::BlendOp::eAdd;
        case BlendOp::Subtract:
            return vk::BlendOp::eSubtract;
        case BlendOp::ReverseSubtract:
            return vk::BlendOp::eReverseSubtract;
        case BlendOp::Min:
            return vk::BlendOp::eMin;
        case BlendOp::Max:
            return vk::BlendOp::eMax;
        default:
            return vk::BlendOp::eAdd;
    }
}

inline constexpr auto to_vk_logic_op(LogicOp op) noexcept -> vk::LogicOp {
    switch (op) {
        case LogicOp::Clear:
            return vk::LogicOp::eClear;
        case LogicOp::And:
            return vk::LogicOp::eAnd;
        case LogicOp::AndReverse:
            return vk::LogicOp::eAndReverse;
        case LogicOp::Copy:
            return vk::LogicOp::eCopy;
        case LogicOp::AndInverted:
            return vk::LogicOp::eAndInverted;
        case LogicOp::NoOp:
            return vk::LogicOp::eNoOp;
        case LogicOp::Xor:
            return vk::LogicOp::eXor;
        case LogicOp::Or:
            return vk::LogicOp::eOr;
        case LogicOp::Nor:
            return vk::LogicOp::eNor;
        case LogicOp::Equiv:
            return vk::LogicOp::eEquivalent;
        case LogicOp::Invert:
            return vk::LogicOp::eInvert;
        case LogicOp::OrReverse:
            return vk::LogicOp::eOrReverse;
        case LogicOp::CopyInverted:
            return vk::LogicOp::eCopyInverted;
        case LogicOp::OrInverted:
            return vk::LogicOp::eOrInverted;
        case LogicOp::Nand:
            return vk::LogicOp::eNand;
        case LogicOp::Set:
            return vk::LogicOp::eSet;
        default:
            return vk::LogicOp::eClear;
    }
}

inline constexpr auto to_vk_comp_op(CompareOp op) noexcept -> vk::CompareOp {
    switch (op) {
        case CompareOp::Never:
            return vk::CompareOp::eNever;
        case CompareOp::Less:
            return vk::CompareOp::eLess;
        case CompareOp::Equal:
            return vk::CompareOp::eEqual;
        case CompareOp::LessEqual:
            return vk::CompareOp::eLessOrEqual;
        case CompareOp::Greater:
            return vk::CompareOp::eGreater;
        case CompareOp::NotEqual:
            return vk::CompareOp::eNotEqual;
        case CompareOp::GreaterEqual:
            return vk::CompareOp::eGreaterOrEqual;
        case CompareOp::Always:
            return vk::CompareOp::eAlways;
        default:
            return vk::CompareOp::eNever;
    }
}

inline constexpr auto to_vk_assembly_state(AssemblyState assembly_state) noexcept {
    return vk::PipelineInputAssemblyStateCreateInfo{
        .topology               = to_vk_primitive_topology(assembly_state.primitive),
        .primitiveRestartEnable = assembly_state.restart_enable,
    };
}

inline constexpr auto to_vk_rasterization_state(RasterizationState rasterization_state) noexcept {
    return vk::PipelineRasterizationStateCreateInfo{
        .depthClampEnable        = rasterization_state.depth_clamp_enable,
        .polygonMode             = to_vk_polygon_mode(rasterization_state.fill_mode),
        .cullMode                = to_vk_cull_mode(rasterization_state.cull_mode),
        .frontFace               = rasterization_state.front_counter_clockwise ? vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise,
        .depthBiasEnable         = rasterization_state.depth_bias_enable,
        .depthBiasConstantFactor = rasterization_state.depth_bias,
        .depthBiasClamp          = rasterization_state.depth_bias_clamp,
        .depthBiasSlopeFactor    = rasterization_state.depth_bias_slope_factor,
        .lineWidth               = 1.0f,
    };
}

inline constexpr auto to_vk_stencil_op(StencilOp op) noexcept -> vk::StencilOp {
    switch (op) {
        case StencilOp::Keep:
            return vk::StencilOp::eKeep;
        case StencilOp::Zero:
            return vk::StencilOp::eZero;
        case StencilOp::Replace:
            return vk::StencilOp::eReplace;
        case StencilOp::IncrementClamp:
            return vk::StencilOp::eIncrementAndClamp;
        case StencilOp::DecrementClamp:
            return vk::StencilOp::eDecrementAndClamp;
        case StencilOp::Invert:
            return vk::StencilOp::eInvert;
        case StencilOp::IncrementWrap:
            return vk::StencilOp::eIncrementAndWrap;
        case StencilOp::DecrementWrap:
            return vk::StencilOp::eDecrementAndWrap;
        default:
            return vk::StencilOp::eKeep;
    }
}

inline constexpr auto to_vk_stencil_op_state(StencilOpState stencil_op_state) noexcept {
    return vk::StencilOpState{
        .failOp      = to_vk_stencil_op(stencil_op_state.fail_op),
        .passOp      = to_vk_stencil_op(stencil_op_state.pass_op),
        .depthFailOp = to_vk_stencil_op(stencil_op_state.depth_fail_op),
        .compareOp   = to_vk_comp_op(stencil_op_state.compare_op),
        .compareMask = stencil_op_state.compare_mask,
        .writeMask   = stencil_op_state.write_mask,
        .reference   = stencil_op_state.reference,
    };
}

inline constexpr auto to_vk_depth_stencil_state(DepthStencilState depth_stencil_state) noexcept {
    return vk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable       = depth_stencil_state.depth_test_enable,
        .depthWriteEnable      = depth_stencil_state.depth_write_enable,
        .depthCompareOp        = to_vk_comp_op(depth_stencil_state.depth_compare_op),
        .depthBoundsTestEnable = depth_stencil_state.depth_bounds_test_enable,
        .stencilTestEnable     = depth_stencil_state.stencil_test_enable,
        .front                 = to_vk_stencil_op_state(depth_stencil_state.front),
        .back                  = to_vk_stencil_op_state(depth_stencil_state.back),
        .minDepthBounds        = depth_stencil_state.depth_bounds.x,
        .maxDepthBounds        = depth_stencil_state.depth_bounds.y,
    };
}

inline constexpr auto to_vk_blend_attachment_state(BlendState blend_state) noexcept {
    return vk::PipelineColorBlendAttachmentState{
        .blendEnable         = blend_state.blend_enable,
        .srcColorBlendFactor = to_vk_blend_factor(blend_state.src_color_blend_factor),
        .dstColorBlendFactor = to_vk_blend_factor(blend_state.dst_color_blend_factor),
        .colorBlendOp        = to_vk_blend_op(blend_state.color_blend_op),
        .srcAlphaBlendFactor = to_vk_blend_factor(blend_state.src_alpha_blend_factor),
        .dstAlphaBlendFactor = to_vk_blend_factor(blend_state.dst_alpha_blend_factor),
        .alphaBlendOp        = to_vk_blend_op(blend_state.alpha_blend_op),
        .colorWriteMask      = to_vk_color_mask(blend_state.color_write_mask),
    };
}

}  // namespace hitagi::gfx