#include "vk_resource.hpp"
#include "vk_device.hpp"
#include "utils.hpp"

#include <hitagi/utils/flags.hpp>

#include <fmt/color.h>
#include <spdlog/logger.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <spirv_reflect.h>

#include <numeric>

namespace hitagi::gfx {

VulkanBuffer::VulkanBuffer(VulkanDevice& device, GPUBufferDesc desc, std::span<const std::byte> initial_data) : GPUBuffer(device, desc) {
    auto logger = device.GetLogger();

    logger->debug("Create buffer({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        vk::BufferCreateInfo buffer_create_info = {
            .size = desc.element_size * desc.element_count,
        };

        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopySrc)) {
            buffer_create_info.usage |= vk::BufferUsageFlagBits::eTransferSrc;
        }
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopyDst)) {
            buffer_create_info.usage |= vk::BufferUsageFlagBits::eTransferDst;
        }
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Vertex)) {
            buffer_create_info.usage |= vk::BufferUsageFlagBits::eVertexBuffer;
        }
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Index)) {
            buffer_create_info.usage |= vk::BufferUsageFlagBits::eIndexBuffer;
        }
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Constant)) {
            buffer_create_info.usage |= vk::BufferUsageFlagBits::eUniformBuffer;
        }

        buffer = std::make_unique<vk::raii::Buffer>(device.GetDevice(), buffer_create_info, device.GetCustomAllocator());
    }

    logger->debug("Allocate buffer({}) memory", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
    {
        VmaAllocationCreateInfo allocation_create_info{};
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapRead) ||
            utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocation_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

            if (utils::has_flag(desc.usages, GPUBufferUsageFlags::Vertex) ||
                utils::has_flag(desc.usages, GPUBufferUsageFlags::Index) ||
                utils::has_flag(desc.usages, GPUBufferUsageFlags::Constant)) {
                allocation_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            }
        } else {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapRead)) {
            allocation_create_info.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            allocation_create_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        VmaAllocationInfo allocation_info;
        if (vmaAllocateMemoryForBuffer(
                static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(),
                **buffer,
                &allocation_create_info,
                &allocation,
                &allocation_info) != VK_SUCCESS) {
            auto error_message = fmt::format("failed to allocate memory for buffer({})", fmt::styled(desc.name, fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
        vmaBindBufferMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation, **buffer);

        logger->debug("Map buffer({}) memory", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
        if (utils::has_flag(desc.usages, GPUBufferUsageFlags::MapRead) ||
            utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            mapped_ptr = static_cast<std::byte*>(allocation_info.pMappedData);
        }
    }

    logger->debug("Copy initial data to buffer({})", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));
    if (!initial_data.empty()) {
        if (initial_data.size() > desc.element_size * desc.element_count) {
            logger->warn(
                "the initial data size({}) is larger than gpu buffer({}) size({}), so the exceed data will not be copied!",
                fmt::styled(initial_data.size(), fmt::fg(fmt::color::red)),
                fmt::styled(desc.element_size * desc.element_count, fmt::fg(fmt::color::green)),
                fmt::styled(desc.name, fmt::fg(fmt::color::green)));
        }
        if (mapped_ptr != nullptr && utils::has_flag(desc.usages, GPUBufferUsageFlags::MapWrite)) {
            std::memcpy(mapped_ptr, initial_data.data(), std::min(initial_data.size(), desc.element_size * desc.element_count));
        } else if (utils::has_flag(desc.usages, GPUBufferUsageFlags::CopyDst)) {
            logger->debug("Using stage buffer to initial...");
            VulkanBuffer staging_buffer(
                device, GPUBufferDesc{
                            .name          = fmt::format("{}_staging", desc.name),
                            .element_size  = desc.element_size * desc.element_count,
                            .element_count = 1,
                            .usages        = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite,
                        },
                initial_data);

            auto  context    = device.CreateCopyContext("StageBufferToGPUBuffer");
            auto& copy_queue = device.GetCommandQueue(CommandType::Copy);

            context->Begin();
            context->CopyBuffer(staging_buffer, 0, *this, 0, desc.element_size * desc.element_count);
            context->End();
            copy_queue.Submit({context.get()});

            // ! improve me
            copy_queue.WaitIdle();
        } else {
            auto error_message = fmt::format(
                "the gpu buffer({}) can not initialize with staging buffer without {}, the actual flags are {}",
                fmt::styled(desc.name, fmt::fg(fmt::color::red)),
                fmt::styled(magic_enum::enum_flags_name(GPUBufferUsageFlags::CopyDst), fmt::fg(fmt::color::green)),
                fmt::styled(magic_enum::enum_flags_name(desc.usages), fmt::fg(fmt::color::red)));
            logger->error(error_message);
            throw std::runtime_error(error_message);
        }
    }

    create_vk_debug_object_info(*buffer, m_Name, device.GetDevice());
}

VulkanBuffer::~VulkanBuffer() {
    vmaFreeMemory(static_cast<VulkanDevice&>(m_Device).GetVmaAllocator(), allocation);
}

auto VulkanBuffer::GetMappedPtr() const noexcept -> std::byte* {
    return mapped_ptr;
}

VulkanImage::VulkanImage(VulkanDevice& device, TextureDesc desc) : Texture(device, desc) {}

VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, SwapChainDesc desc) : SwapChain(device, desc) {
    auto window_size = math::vec2u{};

    switch (desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            auto h_wnd = static_cast<HWND>(desc.window.ptr);

            RECT rect;
            GetClientRect(static_cast<HWND>(desc.window.ptr), &rect);
            window_size.x = rect.right - rect.left;
            window_size.y = rect.bottom - rect.top;

            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
            surface = std::make_unique<vk::raii::SurfaceKHR>(device.GetInstance(), surface_create_info, device.GetCustomAllocator());
        } break;
#endif
        case utils::Window::Type::SDL2: {
            auto sdl_window = static_cast<SDL_Window*>(desc.window.ptr);
            window_size     = get_sdl2_drawable_size(sdl_window);

            SDL_SysWMinfo wm_info;
            SDL_VERSION(&wm_info.version);
            if (!SDL_GetWindowWMInfo(sdl_window, &wm_info)) {
                const auto error_message = fmt::format("SDL_GetWindowWMInfo failed: {}", SDL_GetError());
                device.GetLogger()->error(error_message);
                throw std::runtime_error(error_message);
            }
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            assert(wm_info.subsystem == SDL_SYSWM_WINDOWS);
            HWND                          h_wnd = wm_info.info.win.window;
            vk::Win32SurfaceCreateInfoKHR surface_create_info{
                .hinstance = GetModuleHandle(nullptr),
                .hwnd      = h_wnd,
            };
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            assert(wm_info.subsystem == SDL_SYSWM_WAYLAND);
            vk::WaylandSurfaceCreateInfoKHR surface_create_info{
                .display = wm_info.info.wl.display,
                .surface = wm_info.info.wl.surface,
            };
#endif
            surface = std::make_unique<vk::raii::SurfaceKHR>(device.GetInstance(), surface_create_info, device.GetCustomAllocator());
        } break;
    }

    CreateSwapChain();
    CreateImageViews();
}

auto VulkanSwapChain::GetCurrentBackBuffer() -> Texture& {
    return GetBuffers()[0];
}

auto VulkanSwapChain::GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> {
    std::pmr::vector<std::reference_wrapper<Texture>> result;
    std::transform(images.begin(), images.end(), std::back_inserter(result), [](const auto& image) { return std::ref(*image); });
    return result;
}

auto VulkanSwapChain::Width() -> std::uint32_t {
    return size.x;
}

auto VulkanSwapChain::Height() -> std::uint32_t {
    return size.y;
}

void VulkanSwapChain::Present() {
}

void VulkanSwapChain::Resize() {
    m_Device.WaitIdle();
    CreateSwapChain();
    CreateImageViews();
}

void VulkanSwapChain::CreateSwapChain() {
    swap_chain = nullptr;

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);

    math::vec2u window_size;
    switch (m_Desc.window.type) {
#ifdef _WIN32
        case utils::Window::Type::Win32: {
            auto h_wnd = static_cast<HWND>(desc.window.ptr);

            RECT rect;
            GetClientRect(static_cast<HWND>(desc.window.ptr), &rect);
            window_size.x = rect.right - rect.left;
            window_size.y = rect.bottom - rect.top;
        } break;
#endif
        case utils::Window::Type::SDL2:
            window_size = get_sdl2_drawable_size(static_cast<SDL_Window*>(m_Desc.window.ptr));
            break;
    }

    // we use graphics queue as present queue
    const auto& graphics_queue  = static_cast<VulkanCommandQueue&>(vk_device.GetCommandQueue(CommandType::Graphics));
    const auto& physical_device = vk_device.GetPhysicalDevice();
    if (!physical_device.getSurfaceSupportKHR(graphics_queue.GetFamilyIndex(), **surface)) {
        throw std::runtime_error(fmt::format(
            "The graphics queue({}) of physical device({}) can not support surface(window.ptr: {})",
            graphics_queue.GetFamilyIndex(),
            physical_device.getProperties().deviceName,
            m_Desc.window.ptr));
    }

    const auto surface_capabilities    = physical_device.getSurfaceCapabilitiesKHR(**surface);
    const auto supported_formats       = physical_device.getSurfaceFormatsKHR(**surface);
    const auto supported_present_modes = physical_device.getSurfacePresentModesKHR(**surface);

    if (std::find_if(supported_formats.begin(), supported_formats.end(), [this](const auto& surface_format) {
            return surface_format.format == to_vk_format(m_Desc.format);
        }) == supported_formats.end()) {
        throw std::runtime_error(fmt::format(
            "The physical device({}) can not support surface(window.ptr: {}) with format: {}",
            physical_device.getProperties().deviceName,
            m_Desc.window.ptr,
            magic_enum::enum_name(m_Desc.format)));
    }

    if (std::find_if(supported_present_modes.begin(), supported_present_modes.end(), [](const auto& present_mode) {
            return present_mode == vk::PresentModeKHR::eFifo;
        }) == supported_present_modes.end()) {
        throw std::runtime_error(fmt::format(
            "The physical device({}) can not support surface(window.ptr: {}) with present mode: VK_PRESENT_MODE_FIFO_KHR",
            physical_device.getProperties().deviceName,
            m_Desc.window.ptr));
    }

    if (surface_capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max()) {
        // surface size is undefined, so we define it
        size.x = std::clamp(window_size.x, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        size.y = std::clamp(window_size.y, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    } else {
        // surface size is defined, so we use it
        size.x = surface_capabilities.currentExtent.width;
        size.y = surface_capabilities.currentExtent.height;
    }

    vk::SurfaceTransformFlagBitsKHR preTransform =
        (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
            ? vk::SurfaceTransformFlagBitsKHR::eIdentity
            : surface_capabilities.currentTransform;

    vk::CompositeAlphaFlagBitsKHR composite_alpha =
        (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)    ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
        : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                                                                          : vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface          = **surface,
        .minImageCount    = surface_capabilities.minImageCount,
        .imageFormat      = to_vk_format(m_Desc.format),
        .imageExtent      = vk::Extent2D{size.x, size.y},
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform     = preTransform,
        .compositeAlpha   = composite_alpha,
        .presentMode      = vk::PresentModeKHR::eFifo,
        .clipped          = true,
    };

    // We use graphics queue as present queue, so we do not care the ownership of images
    swap_chain = std::make_unique<vk::raii::SwapchainKHR>(vk_device.GetDevice(), swapchain_create_info, vk_device.GetCustomAllocator());

    create_vk_debug_object_info(*swap_chain, m_Name, vk_device.GetDevice());
}

void VulkanSwapChain::CreateImageViews() {
    images.clear();

    auto& vk_device = static_cast<VulkanDevice&>(m_Device);
    auto  _images   = swap_chain->getImages();

    vk::ImageViewCreateInfo image_view_create_info{
        .viewType         = vk::ImageViewType::e2D,
        .format           = to_vk_format(m_Desc.format),
        .subresourceRange = {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    TextureDesc texture_desc{
        .width  = size.x,
        .height = size.y,
        .format = m_Desc.format,
        .usages = TextureUsageFlags::RTV,
    };

    for (std::size_t i = 0; i < _images.size(); i++) {
        auto name = fmt::format("{}-image-{}", m_Desc.name, i);

        texture_desc.name = name;

        images.emplace_back(std::make_shared<VulkanImage>(vk_device, texture_desc));

        image_view_create_info.image = _images[i];
        images.back()->image_view    = vk::raii::ImageView(vk_device.GetDevice(), image_view_create_info, vk_device.GetCustomAllocator());
    }
}

VulkanShader::VulkanShader(VulkanDevice& device, ShaderDesc desc, std::span<const std::byte> _binary_program)
    : Shader(device, desc),
      binary_program(_binary_program.empty() ? device.CompileShader(m_Desc) : _binary_program),
      shader(
          device.GetDevice(),
          {
              .codeSize = binary_program.GetDataSize(),
              .pCode    = reinterpret_cast<const std::uint32_t*>(binary_program.GetData()),
          },
          device.GetCustomAllocator())

{
    create_vk_debug_object_info(shader, m_Name, device.GetDevice());
}

auto VulkanShader::GetSPIRVData() const noexcept -> std::span<const std::byte> {
    return binary_program.Span<const std::byte>();
}

VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice& device, RootSignatureDesc desc)
    : RootSignature(device, std::move(desc)) {
    auto logger = device.GetLogger();

    logger->debug("Creating pipeline layout {} ...", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));

    logger->debug("Create reflection data from shaders");
    std::pmr::vector<spv_reflect::ShaderModule> reflections;
    for (const auto& _shader : m_Desc.shaders) {
        auto shader = _shader.lock();
        if (shader == nullptr) continue;

        auto spirv_data = shader->GetSPIRVData();
        reflections.emplace_back(spirv_data.size_bytes(), spirv_data.data());
    }

    logger->debug("Enumerate descriptor sets...");
    std::pmr::set<std::uint32_t> set_indices;
    for (const auto& reflection : reflections) {
        std::uint32_t num_sets;
        reflection.EnumerateDescriptorSets(&num_sets, nullptr);
        std::pmr::vector<SpvReflectDescriptorSet*> sets(num_sets);
        reflection.EnumerateDescriptorSets(&num_sets, sets.data());

        for (const auto& set : sets) set_indices.insert(set->set);
    }

    // Verify set index
    {
        std::uint32_t i = 0;
        for (auto set_index : set_indices) {
            if (set_index != i) throw std::runtime_error("Set index is not continuous");
            i++;
        }
    }

    logger->debug("Create bindings for each descriptor set layout...");
    for (auto set_index : set_indices) {
        std::pmr::vector<vk::DescriptorSetLayoutBinding> bindings;
        for (const auto& reflection : reflections) {
            auto spv_set = reflection.GetEntryPointDescriptorSet(reflection.GetEntryPointName(), set_index);
            if (spv_set == nullptr) continue;

            auto spv_bindings = std::span(spv_set->bindings, spv_set->binding_count);

            for (const auto& spv_binding : spv_bindings) {
                if (auto iter = std::find_if(
                        bindings.begin(), bindings.end(),
                        [&](const auto& binding) {
                            return binding.binding == spv_binding->binding &&
                                   binding.descriptorType != to_vk_descriptor_type(spv_binding->descriptor_type);
                        });
                    iter != bindings.end()) {
                    if (iter->descriptorCount != spv_binding->count)
                        throw std::runtime_error("Descriptor count is not same");

                    iter->stageFlags |= to_vk_shader_stage(reflection.GetShaderStage());
                    logger->debug("Merge shader stage to {}", vk::to_string(iter->stageFlags));
                } else {
                    const auto& binding = bindings.emplace_back(vk::DescriptorSetLayoutBinding{
                        .binding            = spv_binding->binding,
                        .descriptorType     = to_vk_descriptor_type(spv_binding->descriptor_type),
                        .descriptorCount    = spv_binding->count,
                        .stageFlags         = to_vk_shader_stage(reflection.GetShaderStage()),
                        .pImmutableSamplers = nullptr,
                    });
                    logger->debug("Create binding {} for stage {} in set {}", binding.binding, vk::to_string(binding.stageFlags), set_index);
                }
            }
        }

        const auto& descriptor_set_layout = descriptor_set_layouts.emplace_back(
            device.GetDevice(),
            vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                .pBindings    = bindings.data(),
                .flags        = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            },
            device.GetCustomAllocator());

        create_vk_debug_object_info(descriptor_set_layout, fmt::format("{}_Descriptor_Set_Layout_{}", m_Desc.name, set_index), device.GetDevice());
    }

    std::pmr::vector<vk::DescriptorSetLayout> vk_descriptor_set_layouts;
    std::transform(
        descriptor_set_layouts.begin(), descriptor_set_layouts.end(),
        std::back_inserter(vk_descriptor_set_layouts),
        [](const auto& layout) { return *layout; });

    pipeline_layout = std::make_unique<vk::raii::PipelineLayout>(
        device.GetDevice(),
        vk::PipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<std::uint32_t>(vk_descriptor_set_layouts.size()),
            .pSetLayouts    = vk_descriptor_set_layouts.data(),
        },
        device.GetCustomAllocator());

    create_vk_debug_object_info(*pipeline_layout, m_Desc.name, device.GetDevice());
}

VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice& device, GraphicsPipelineDesc desc)
    : GraphicsPipeline(device, std::move(desc))

{
    auto logger = device.GetLogger();

    if (std::find_if(
            m_Desc.shaders.begin(), m_Desc.shaders.end(),
            [](const auto& _shader) {
                auto shader = _shader.lock();
                return shader && shader->GetDesc().type == ShaderType::Vertex;
            }) == m_Desc.shaders.end()) {
        auto error_message = fmt::format(
            "Vertex shader is not specified for pipeline({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    std::pmr::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos;
    std::transform(
        m_Desc.shaders.begin(), m_Desc.shaders.end(), std::back_inserter(shader_stage_create_infos),
        [](const auto& _shader) {
            auto shader = _shader.lock();
            return vk::PipelineShaderStageCreateInfo{
                .stage  = to_vk_shader_stage(shader->GetDesc().type),
                .module = *std::static_pointer_cast<VulkanShader>(shader)->shader,
                .pName  = shader->GetDesc().entry.data(),
            };
        });

    const auto assembly_state = to_vk_assembly_state(m_Desc.assembly_state);

    std::pmr::vector<vk::VertexInputBindingDescription>   vertex_input_binding_descriptions;
    std::pmr::vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions;
    // Then sort all semantic strings according to declaration and assign Location numbers sequentially to the corresponding SPIR-V variables.
    // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#implicit-location-number-assignment
    for (std::uint32_t binding = 0, location = 0; binding < m_Desc.vertex_input_layout.size(); binding++) {
        vertex_input_binding_descriptions.emplace_back(
            vk::VertexInputBindingDescription{
                .binding   = binding,
                .stride    = m_Desc.vertex_input_layout[binding].stride,
                .inputRate = m_Desc.vertex_input_layout[binding].per_instance
                                 ? vk::VertexInputRate::eInstance
                                 : vk::VertexInputRate::eVertex,
            });
        for (const auto& attribute : m_Desc.vertex_input_layout[binding].attributes) {
            vertex_input_attribute_descriptions.emplace_back(
                vk::VertexInputAttributeDescription{
                    .location = location++,
                    .binding  = binding,
                    .format   = to_vk_format(attribute.format),
                    .offset   = attribute.offset,
                });
        }
    }
    const vk::PipelineVertexInputStateCreateInfo vertex_state = {
        .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(vertex_input_binding_descriptions.size()),
        .pVertexBindingDescriptions      = vertex_input_binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertex_input_attribute_descriptions.size()),
        .pVertexAttributeDescriptions    = vertex_input_attribute_descriptions.data(),
    };
    const auto viewport_state      = vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};
    const auto rasterization_state = to_vk_rasterization_state(m_Desc.rasterization_state);
    const auto depth_stencil_state = to_vk_depth_stencil_state(m_Desc.depth_stencil_state);
    const auto attachment_state    = to_vk_blend_attachment_state(m_Desc.blend_state);

    const vk::PipelineColorBlendStateCreateInfo blend_state{
        .logicOpEnable   = m_Desc.blend_state.logic_operation_enable,
        .logicOp         = to_vk_logic_op(m_Desc.blend_state.logic_op),
        .attachmentCount = 1,
        .pAttachments    = &attachment_state,
        .blendConstants  = m_Desc.blend_state.blend_constants.data,
    };

    std::array dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{
        .dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };

    const auto render_format = to_vk_format(desc.render_format);

    auto root_signature = std::static_pointer_cast<VulkanPipelineLayout>(m_Desc.root_signature.lock());
    if (root_signature == nullptr) {
        auto error_message = fmt::format(
            "Root signature is not specified for pipeline({})",
            fmt::styled(m_Desc.name, fmt::fg(fmt::color::red)));
        logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    const auto& pipeline_layout = root_signature->pipeline_layout;

    logger->debug("Create pipeline({})", fmt::styled(m_Desc.name, fmt::fg(fmt::color::green)));
    {
        vk::StructureChain pipeline_create_info = {
            vk::GraphicsPipelineCreateInfo{
                .stageCount          = static_cast<std::uint32_t>(shader_stage_create_infos.size()),
                .pStages             = shader_stage_create_infos.data(),
                .pVertexInputState   = &vertex_state,
                .pInputAssemblyState = &assembly_state,
                .pViewportState      = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pDepthStencilState  = &depth_stencil_state,
                .pColorBlendState    = &blend_state,
                .pDynamicState       = &dynamic_state_create_info,
                .layout              = **pipeline_layout,
                .renderPass          = nullptr,
            },
            vk::PipelineRenderingCreateInfo{
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &render_format,
            },
        };

        pipeline = std::make_unique<vk::raii::Pipeline>(device.GetDevice(), nullptr, pipeline_create_info.get(), device.GetCustomAllocator());

        switch (pipeline->getConstructorSuccessCode()) {
            case vk::Result::eSuccess:
                break;
            case vk::Result::ePipelineCompileRequired:
                throw std::runtime_error("Pipeline compilation is required");
            default:
                throw std::runtime_error("Failed to create pipeline");
        }
        create_vk_debug_object_info(*pipeline, m_Name, device.GetDevice());
    }
}

}  // namespace hitagi::gfx