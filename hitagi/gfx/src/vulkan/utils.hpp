#pragma once
#include "vk_configs.hpp"
#include "vk_resource.hpp"

#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/utils/array.hpp>
#include <hitagi/utils/soa.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <SDL2/SDL_vulkan.h>
#include <spirv_reflect.h>

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
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
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
#ifdef HITAGI_DEBUG
    vk_device.setDebugUtilsObjectNameEXT({
        .objectType   = obj.objectType,
        .objectHandle = reinterpret_cast<std::uintptr_t>(static_cast<typename T::CType>(*obj)),
        .pObjectName  = name.data(),
    });
#endif
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

inline constexpr auto from_vk_format(vk::Format format) noexcept -> Format {
    switch (format) {
        case vk::Format::eR32G32B32A32Sfloat:
            return Format::R32G32B32A32_FLOAT;
        case vk::Format::eR32G32B32A32Uint:
            return Format::R32G32B32A32_UINT;
        case vk::Format::eR32G32B32A32Sint:
            return Format::R32G32B32A32_SINT;
        case vk::Format::eR32G32B32Sfloat:
            return Format::R32G32B32_FLOAT;
        case vk::Format::eR32G32B32Uint:
            return Format::R32G32B32_UINT;
        case vk::Format::eR32G32B32Sint:
            return Format::R32G32B32_SINT;
        case vk::Format::eR16G16B16A16Sfloat:
            return Format::R16G16B16A16_FLOAT;
        case vk::Format::eR16G16B16A16Unorm:
            return Format::R16G16B16A16_UNORM;
        case vk::Format::eR16G16B16A16Uint:
            return Format::R16G16B16A16_UINT;
        case vk::Format::eR16G16B16A16Snorm:
            return Format::R16G16B16A16_SNORM;
        case vk::Format::eR16G16B16A16Sint:
            return Format::R16G16B16A16_SINT;
        case vk::Format::eR32G32Sfloat:
            return Format::R32G32_FLOAT;
        case vk::Format::eR32G32Uint:
            return Format::R32G32_UINT;
        case vk::Format::eR32G32Sint:
            return Format::R32G32_SINT;
        case vk::Format::eD32SfloatS8Uint:
            return Format::D32_FLOAT_S8X24_UINT;
        case vk::Format::eB10G11R11UfloatPack32:
            return Format::R11G11B10_FLOAT;
        case vk::Format::eR8G8B8A8Unorm:
            return Format::R8G8B8A8_UNORM;
        case vk::Format::eR8G8B8A8Srgb:
            return Format::R8G8B8A8_UNORM_SRGB;
        case vk::Format::eR8G8B8A8Uint:
            return Format::R8G8B8A8_UINT;
        case vk::Format::eR8G8B8A8Snorm:
            return Format::R8G8B8A8_SNORM;
        case vk::Format::eR8G8B8A8Sint:
            return Format::R8G8B8A8_SINT;
        case vk::Format::eR16G16Sfloat:
            return Format::R16G16_FLOAT;
        case vk::Format::eR16G16Unorm:
            return Format::R16G16_UNORM;
        case vk::Format::eR16G16Uint:
            return Format::R16G16_UINT;
        case vk::Format::eR16G16Snorm:
            return Format::R16G16_SNORM;
        case vk::Format::eR16G16Sint:
            return Format::R16G16_SINT;
        case vk::Format::eD32Sfloat:
            return Format::D32_FLOAT;
        case vk::Format::eR32Sfloat:
            return Format::R32_FLOAT;
        case vk::Format::eR32Uint:
            return Format::R32_UINT;
        case vk::Format::eR32Sint:
            return Format::R32_SINT;
        case vk::Format::eD24UnormS8Uint:
            return Format::D24_UNORM_S8_UINT;
        case vk::Format::eR8G8Unorm:
            return Format::R8G8_UNORM;
        case vk::Format::eR8G8Uint:
            return Format::R8G8_UINT;
        case vk::Format::eR8G8Snorm:
            return Format::R8G8_SNORM;
        case vk::Format::eR8G8Sint:
            return Format::R8G8_SINT;
        case vk::Format::eR16Sfloat:
            return Format::R16_FLOAT;
        case vk::Format::eD16Unorm:
            return Format::D16_UNORM;
        case vk::Format::eR16Unorm:
            return Format::R16_UNORM;
        case vk::Format::eR16Uint:
            return Format::R16_UINT;
        case vk::Format::eR16Snorm:
            return Format::R16_SNORM;
        case vk::Format::eR16Sint:
            return Format::R16_SINT;
        case vk::Format::eR8Unorm:
            return Format::R8_UNORM;
        case vk::Format::eR8Uint:
            return Format::R8_UINT;
        case vk::Format::eR8Snorm:
            return Format::R8_SNORM;
        case vk::Format::eR8Sint:
            return Format::R8_SINT;
        case vk::Format::eB5G6R5UnormPack16:
            return Format::B5G6R5_UNORM;
        case vk::Format::eB5G5R5A1UnormPack16:
            return Format::B5G5R5A1_UNORM;
        case vk::Format::eB8G8R8A8Unorm:
            return Format::B8G8R8A8_UNORM;
        case vk::Format::eB8G8R8A8Srgb:
            return Format::B8G8R8A8_UNORM_SRGB;
        case vk::Format::eBc6HUfloatBlock:
            return Format::BC6H_UF16;
        case vk::Format::eBc6HSfloatBlock:
            return Format::BC6H_SF16;
        case vk::Format::eBc7UnormBlock:
            return Format::BC7_UNORM;
        case vk::Format::eBc7SrgbBlock:
            return Format::BC7_UNORM_SRGB;
        default:
            return Format::UNKNOWN;
    }
}

inline constexpr auto to_vk_address_mode(AddressMode mode) noexcept -> vk::SamplerAddressMode {
    switch (mode) {
        case AddressMode::Clamp:
            return vk::SamplerAddressMode::eClampToEdge;
        case AddressMode::Repeat:
            return vk::SamplerAddressMode::eRepeat;
        case AddressMode::MirrorRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        default:
            return vk::SamplerAddressMode::eClampToEdge;
    }
}

inline constexpr auto to_vk_filter_mode(FilterMode mode) noexcept -> vk::Filter {
    switch (mode) {
        case FilterMode::Point:
            return vk::Filter::eNearest;
        case FilterMode::Linear:
            return vk::Filter::eLinear;
        default:
            return vk::Filter::eNearest;
    }
}

inline constexpr auto to_vk_mipmap_filter_mode(FilterMode mode) noexcept -> vk::SamplerMipmapMode {
    switch (mode) {
        case FilterMode::Point:
            return vk::SamplerMipmapMode::eNearest;
        case FilterMode::Linear:
            return vk::SamplerMipmapMode::eLinear;
        default:
            return vk::SamplerMipmapMode::eNearest;
    }
}

inline constexpr auto to_vk_extent3D(math::vec3u value) noexcept -> vk::Extent3D {
    return {value.x, value.y, value.z};
};

inline constexpr auto to_vk_offset3D(math::vec3i value) noexcept -> vk::Offset3D {
    return {value.x, value.y, value.z};
};

inline constexpr auto to_vk_buffer_usage(GPUBufferUsageFlags usages) noexcept -> vk::BufferUsageFlags {
    vk::BufferUsageFlags vk_usages;

    if (utils::has_flag(usages, GPUBufferUsageFlags::CopySrc)) {
        vk_usages |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    if (utils::has_flag(usages, GPUBufferUsageFlags::CopyDst)) {
        vk_usages |= vk::BufferUsageFlagBits::eTransferDst;
    }
    if (utils::has_flag(usages, GPUBufferUsageFlags::Vertex)) {
        vk_usages |= vk::BufferUsageFlagBits::eVertexBuffer;
    }
    if (utils::has_flag(usages, GPUBufferUsageFlags::Index)) {
        vk_usages |= vk::BufferUsageFlagBits::eIndexBuffer;
    }
    if (utils::has_flag(usages, GPUBufferUsageFlags::Constant)) {
        vk_usages |= vk::BufferUsageFlagBits::eUniformBuffer;
        // TODO for now we use storage to simulate uniform buffer for bindless usage
        vk_usages |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    if (utils::has_flag(usages, GPUBufferUsageFlags::Storage)) {
        vk_usages |= vk::BufferUsageFlagBits::eStorageBuffer;
    }
    return vk_usages;
}

inline constexpr auto to_vk_image_usage(TextureUsageFlags usages) noexcept -> vk::ImageUsageFlags {
    vk::ImageUsageFlags vk_usages;
    if (utils::has_flag(usages, TextureUsageFlags::CopySrc)) {
        vk_usages |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    if (utils::has_flag(usages, TextureUsageFlags::CopyDst)) {
        vk_usages |= vk::ImageUsageFlagBits::eTransferDst;
    }
    if (utils::has_flag(usages, TextureUsageFlags::SRV)) {
        vk_usages |= vk::ImageUsageFlagBits::eSampled;
    }
    if (utils::has_flag(usages, TextureUsageFlags::UAV)) {
        vk_usages |= vk::ImageUsageFlagBits::eStorage;
    }
    if (utils::has_flag(usages, TextureUsageFlags::RTV)) {
        vk_usages |= vk::ImageUsageFlagBits::eColorAttachment;
    }
    if (utils::has_flag(usages, TextureUsageFlags::DSV)) {
        vk_usages |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    }
    return vk_usages;
}

inline constexpr auto to_vk_compare_op(CompareOp op) noexcept -> vk::CompareOp {
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

inline constexpr auto to_vk_sampler_create_info(const SamplerDesc& desc) noexcept -> vk::SamplerCreateInfo {
    return {
        .magFilter     = to_vk_filter_mode(desc.mag_filter),
        .minFilter     = to_vk_filter_mode(desc.min_filter),
        .mipmapMode    = to_vk_mipmap_filter_mode(desc.mipmap_filter),
        .addressModeU  = to_vk_address_mode(desc.address_u),
        .addressModeV  = to_vk_address_mode(desc.address_v),
        .addressModeW  = to_vk_address_mode(desc.address_w),
        .maxAnisotropy = desc.max_anisotropy,
        .compareOp     = to_vk_compare_op(desc.compare_op),
        .minLod        = desc.min_lod,
        .maxLod        = desc.max_lod,
    };
}

inline constexpr auto to_vk_shader_stage(ShaderType type) noexcept -> vk::ShaderStageFlagBits {
    switch (type) {
        case ShaderType::Vertex:
            return vk::ShaderStageFlagBits::eVertex;
        case ShaderType::Pixel:
            return vk::ShaderStageFlagBits::eFragment;
        case ShaderType::Geometry:
            return vk::ShaderStageFlagBits::eGeometry;
        case ShaderType::Compute:
            return vk::ShaderStageFlagBits::eCompute;
    }
}

inline constexpr auto to_vk_shader_stage(SpvReflectShaderStageFlagBits stage) noexcept -> vk::ShaderStageFlagBits {
    switch (stage) {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return vk::ShaderStageFlagBits::eVertex;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return vk::ShaderStageFlagBits::eFragment;
        case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
            return vk::ShaderStageFlagBits::eGeometry;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return vk::ShaderStageFlagBits::eCompute;
        default:
            return vk::ShaderStageFlagBits::eAll;
    }
}

inline constexpr auto to_vk_descriptor_type(SpvReflectDescriptorType type) noexcept -> vk::DescriptorType {
    switch (type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            return vk::DescriptorType::eSampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return vk::DescriptorType::eCombinedImageSampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            return vk::DescriptorType::eSampledImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            return vk::DescriptorType::eStorageImage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            return vk::DescriptorType::eUniformTexelBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return vk::DescriptorType::eStorageTexelBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return vk::DescriptorType::eUniformBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return vk::DescriptorType::eStorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            return vk::DescriptorType::eUniformBufferDynamic;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return vk::DescriptorType::eStorageBufferDynamic;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return vk::DescriptorType::eInputAttachment;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            return vk::DescriptorType::eAccelerationStructureKHR;
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
        .compareOp   = to_vk_compare_op(stencil_op_state.compare_op),
        .compareMask = stencil_op_state.compare_mask,
        .writeMask   = stencil_op_state.write_mask,
        .reference   = stencil_op_state.reference,
    };
}

inline constexpr auto to_vk_depth_stencil_state(DepthStencilState depth_stencil_state) noexcept {
    return vk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable       = depth_stencil_state.depth_test_enable,
        .depthWriteEnable      = depth_stencil_state.depth_write_enable,
        .depthCompareOp        = to_vk_compare_op(depth_stencil_state.depth_compare_op),
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

inline constexpr auto to_vk_viewport(ViewPort view_port) noexcept {
    // we need flip viewport here, since NDC is y-up in our engine .
    return vk::Viewport{
        .x        = view_port.x,
        .y        = view_port.y + view_port.height,
        .width    = view_port.width,
        .height   = -view_port.height,
        .minDepth = view_port.min_depth,
        .maxDepth = view_port.max_depth,
    };
}

inline constexpr auto to_vk_clear_value(ClearValue clear_value) noexcept -> vk::ClearValue {
    if (std::holds_alternative<ClearColor>(clear_value)) {
        return {
            .color = {.float32 = std::get<ClearColor>(clear_value).data},
        };
    } else {
        return {
            .depthStencil = {.depth = std::get<ClearDepthStencil>(clear_value).depth, .stencil = std::get<ClearDepthStencil>(clear_value).stencil},
        };
    }
}

inline constexpr auto to_vk_access_flags(BarrierAccess access) noexcept -> vk::AccessFlags2 {
    vk::AccessFlags2 result = vk::AccessFlagBits2::eNone;
    if (access == BarrierAccess::Unkown) {
        return result;
    }
    if (utils::has_flag(access, BarrierAccess::CopySrc)) {
        result |= vk::AccessFlagBits2::eTransferRead;
    }
    if (utils::has_flag(access, BarrierAccess::CopyDst)) {
        result |= vk::AccessFlagBits2::eTransferWrite;
    }
    if (utils::has_flag(access, BarrierAccess::Index)) {
        result |= vk::AccessFlagBits2::eIndexRead;
    }
    if (utils::has_flag(access, BarrierAccess::Vertex)) {
        result |= vk::AccessFlagBits2::eVertexAttributeRead;
    }
    if (utils::has_flag(access, BarrierAccess::Constant)) {
        result |= vk::AccessFlagBits2::eUniformRead;
    }
    if (utils::has_flag(access, BarrierAccess::ShaderRead)) {
        result |= vk::AccessFlagBits2::eShaderRead;
    }
    if (utils::has_flag(access, BarrierAccess::ShaderWrite)) {
        result |= vk::AccessFlagBits2::eShaderWrite;
    }
    if (utils::has_flag(access, BarrierAccess::RenderTarget)) {
        result |= vk::AccessFlagBits2::eColorAttachmentWrite;
    }
    if (utils::has_flag(access, BarrierAccess::Present)) {
        result |= vk::AccessFlagBits2::eMemoryRead;
    }
    return result;
}

inline constexpr auto to_vk_pipeline_stage1(PipelineStage stage) noexcept -> vk::PipelineStageFlags {
    if (utils::has_flag(stage, PipelineStage::All)) {
        return vk::PipelineStageFlagBits::eAllCommands;
    }
    vk::PipelineStageFlags result = vk::PipelineStageFlagBits::eNone;
    if (utils::has_flag(stage, PipelineStage::VertexInput)) {
        result |= vk::PipelineStageFlagBits::eVertexInput;
    }
    if (utils::has_flag(stage, PipelineStage::VertexShader)) {
        result |= vk::PipelineStageFlagBits::eVertexShader;
    }
    if (utils::has_flag(stage, PipelineStage::PixelShader)) {
        result |= vk::PipelineStageFlagBits::eFragmentShader;
    }
    if (utils::has_flag(stage, PipelineStage::DepthStencil)) {
        result |= vk::PipelineStageFlagBits::eLateFragmentTests;
    }
    if (utils::has_flag(stage, PipelineStage::Copy)) {
        result |= vk::PipelineStageFlagBits::eTransfer;
    }
    if (utils::has_flag(stage, PipelineStage::Resolve)) {
        result |= vk::PipelineStageFlagBits::eTransfer;
    }
    if (utils::has_flag(stage, PipelineStage::Render)) {
        result |= vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }
    if (utils::has_flag(stage, PipelineStage::AllGraphics)) {
        result |= vk::PipelineStageFlagBits::eAllGraphics;
    }
    if (utils::has_flag(stage, PipelineStage::ComputeShader)) {
        result |= vk::PipelineStageFlagBits::eComputeShader;
    }
    return result;
}

inline constexpr auto to_vk_pipeline_stage2(PipelineStage stage) noexcept -> vk::PipelineStageFlags2 {
    if (utils::has_flag(stage, PipelineStage::All)) {
        return vk::PipelineStageFlagBits2::eAllCommands;
    }
    vk::PipelineStageFlags2 result = vk::PipelineStageFlagBits2::eNone;
    if (utils::has_flag(stage, PipelineStage::VertexInput)) {
        result |= vk::PipelineStageFlagBits2::eVertexInput;
    }
    if (utils::has_flag(stage, PipelineStage::VertexShader)) {
        result |= vk::PipelineStageFlagBits2::eVertexShader;
    }
    if (utils::has_flag(stage, PipelineStage::PixelShader)) {
        result |= vk::PipelineStageFlagBits2::eFragmentShader;
    }
    if (utils::has_flag(stage, PipelineStage::DepthStencil)) {
        result |= vk::PipelineStageFlagBits2::eLateFragmentTests;
    }
    if (utils::has_flag(stage, PipelineStage::Render)) {
        result |= vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    }
    if (utils::has_flag(stage, PipelineStage::AllGraphics)) {
        result |= vk::PipelineStageFlagBits2::eAllGraphics;
    }
    if (utils::has_flag(stage, PipelineStage::ComputeShader)) {
        result |= vk::PipelineStageFlagBits2::eComputeShader;
    }
    if (utils::has_flag(stage, PipelineStage::Copy)) {
        result |= vk::PipelineStageFlagBits2::eCopy;
    }
    if (utils::has_flag(stage, PipelineStage::Resolve)) {
        result |= vk::PipelineStageFlagBits2::eResolve;
    }
    return result;
}

inline constexpr auto to_vk_image_aspect(TextureUsageFlags usages) -> vk::ImageAspectFlags {
    vk::ImageAspectFlags result = vk::ImageAspectFlagBits::eNone;
    if (utils::has_flag(usages, TextureUsageFlags::DSV)) {
        result |= (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    }
    if (utils::has_flag(usages, TextureUsageFlags::RTV) ||
        utils::has_flag(usages, TextureUsageFlags::SRV) ||
        utils::has_flag(usages, TextureUsageFlags::UAV)) {
        result |= vk::ImageAspectFlagBits::eColor;
    }
    return result;
}

inline constexpr auto to_vk_image_layout(TextureLayout layout) noexcept -> vk::ImageLayout {
    switch (layout) {
        case TextureLayout::Unkown:
            return vk::ImageLayout::eUndefined;
        case TextureLayout::Common:
            return vk::ImageLayout::eGeneral;
        case TextureLayout::CopySrc:
            return vk::ImageLayout::eTransferSrcOptimal;
        case TextureLayout::CopyDst:
            return vk::ImageLayout::eTransferDstOptimal;
        case TextureLayout::ShaderRead:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case TextureLayout::ShaderWrite:
            return vk::ImageLayout::eGeneral;
        case TextureLayout::DepthStencilRead:
            return vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        case TextureLayout::DepthStencilWrite:
            return vk::ImageLayout::eDepthStencilAttachmentOptimal;
        case TextureLayout::RenderTarget:
        case TextureLayout::ResolveSrc:
        case TextureLayout::ResolveDst:
            return vk::ImageLayout::eColorAttachmentOptimal;
        case TextureLayout::Present:
            return vk::ImageLayout::ePresentSrcKHR;
        default:
            return vk::ImageLayout::eUndefined;
    }
}

inline constexpr auto to_vk_image_type(const TextureDesc& desc) noexcept -> vk::ImageType {
    if (desc.height == 1 && desc.depth == 1) {
        return vk::ImageType::e1D;
    } else if (desc.depth == 1) {
        return vk::ImageType::e2D;
    } else {
        return vk::ImageType::e3D;
    }
}

inline constexpr auto to_vk_image_create_info(const TextureDesc& desc) noexcept -> vk::ImageCreateInfo {
    return {
        .imageType = to_vk_image_type(desc),
        .format    = to_vk_format(desc.format),
        .extent    = vk::Extent3D{
               .width  = desc.width,
               .height = desc.height,
               .depth  = desc.depth,
        },
        .mipLevels     = desc.mip_levels,
        .arrayLayers   = desc.array_size,
        .samples       = vk::SampleCountFlagBits::e1,
        .tiling        = vk::ImageTiling::eOptimal,
        .usage         = to_vk_image_usage(desc.usages),
        .sharingMode   = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };
};

inline constexpr auto to_vk_image_view_type(const TextureDesc& desc) noexcept -> vk::ImageViewType {
    vk::ImageViewType image_view_type;
    if (desc.height == 1 && desc.depth == 1) {
        image_view_type = desc.array_size == 1 ? vk::ImageViewType::e1D : vk::ImageViewType::e1DArray;
    } else if (desc.depth == 1) {
        image_view_type = desc.array_size == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
    } else {
        image_view_type = vk::ImageViewType::e3D;
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::Cube)) {
        image_view_type = vk::ImageViewType::eCube;
    }
    if (utils::has_flag(desc.usages, TextureUsageFlags::CubeArray)) {
        image_view_type = vk::ImageViewType::eCubeArray;
    }
    return image_view_type;
}

inline constexpr auto to_vk_image_view_create_info(const TextureDesc& desc) noexcept -> vk::ImageViewCreateInfo {
    return {
        .image            = nullptr,
        .viewType         = to_vk_image_view_type(desc),
        .format           = to_vk_format(desc.format),
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask     = to_vk_image_aspect(desc.usages),
            .baseMipLevel   = 0,
            .levelCount     = desc.mip_levels,
            .baseArrayLayer = 0,
            .layerCount     = desc.array_size,
        },
    };
}

inline constexpr auto to_vk_memory_barrier(GlobalBarrier barrier) noexcept -> vk::MemoryBarrier2 {
    return {
        .srcStageMask  = to_vk_pipeline_stage2(barrier.src_stage),
        .srcAccessMask = to_vk_access_flags(barrier.src_access),
        .dstStageMask  = to_vk_pipeline_stage2(barrier.dst_stage),
        .dstAccessMask = to_vk_access_flags(barrier.dst_access),
    };
}

inline auto to_vk_buffer_barrier(const GPUBufferBarrier& barrier) -> vk::BufferMemoryBarrier2 {
    const auto& vk_buffer = static_cast<VulkanBuffer&>(barrier.buffer);
    return {
        .srcStageMask  = to_vk_pipeline_stage2(barrier.src_stage),
        .srcAccessMask = to_vk_access_flags(barrier.src_access),
        .dstStageMask  = to_vk_pipeline_stage2(barrier.dst_stage),
        .dstAccessMask = to_vk_access_flags(barrier.dst_access),
        .buffer        = **vk_buffer.buffer,
        .offset        = 0,
        .size          = vk_buffer.GetDesc().element_size * vk_buffer.GetDesc().element_count,
    };
}

inline auto to_vk_image_barrier(const TextureBarrier& barrier) -> vk::ImageMemoryBarrier2 {
    const auto vk_image = static_cast<VulkanImage&>(barrier.texture).image_handle;

    return {
        .srcStageMask     = to_vk_pipeline_stage2(barrier.src_stage),
        .srcAccessMask    = to_vk_access_flags(barrier.src_access),
        .dstStageMask     = to_vk_pipeline_stage2(barrier.dst_stage),
        .dstAccessMask    = to_vk_access_flags(barrier.dst_access),
        .oldLayout        = to_vk_image_layout(barrier.src_layout),
        .newLayout        = to_vk_image_layout(barrier.dst_layout),
        .image            = vk_image,
        .subresourceRange = {
            .aspectMask     = to_vk_image_aspect(barrier.texture.GetDesc().usages),
            .baseMipLevel   = barrier.base_mip_level,
            .levelCount     = barrier.level_count,
            .baseArrayLayer = barrier.base_array_layer,
            .layerCount     = barrier.layer_count,
        }};
}

}  // namespace hitagi::gfx