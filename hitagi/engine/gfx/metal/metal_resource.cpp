export module gfx.metal:resource;
import interop.metal;
import interop.sdl;
import interop.spirv_cross;
import interop.spdlog;

import std;
import core;
import utils;
import math;
import gfx.base;
import :types;
import :utils;
import :configs;
import :bindless;

export namespace hitagi::gfx {

struct MetalBuffer final : public GPUBuffer {
    MetalBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, GPUBufferDesc desc, std::span<const std::byte> initial_data);
    auto GetAllocationSize() const noexcept -> std::uint64_t final;
    auto Map() -> std::byte* final;
    void UnMap() final;

    const ns::SharedPtr<mtl::Device>& native_device;
    ns::SharedPtr<mtl::Buffer>        native;
};

struct MetalBufferView final : public GPUBufferView {
    MetalBufferView(MetalBindlessUtils& bindings, GPUBufferViewDesc desc);
};

struct MetalTexture final : public Texture {
    MetalTexture(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data);
    MetalTexture(const ns::SharedPtr<mtl::Device>& device, TextureDesc desc, mtl::Texture* texture);
    auto GetAllocationSize() const noexcept -> std::uint64_t final;

    const ns::SharedPtr<mtl::Device>& native_device;
    ns::SharedPtr<mtl::Texture>       native;
};

struct MetalTextureView final : public TextureView {
    MetalTextureView(MetalBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc);
    ns::SharedPtr<mtl::Texture> native;
};

struct MetalSampler final : public Sampler {
    MetalSampler(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, SamplerDesc desc);
    ns::SharedPtr<mtl::SamplerState> native;
};

struct MetalShader final : public Shader {
    MetalShader(mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ShaderDesc desc, CompiledShader compiled);
    ns::SharedPtr<mtl4::LibraryFunctionDescriptor> descriptor;
    ns::SharedPtr<mtl::Library>                    library;
    ns::SharedPtr<mtl::Function>                   function;
};

struct MetalRenderPipeline final : public RenderPipeline {
    MetalRenderPipeline(const ns::SharedPtr<mtl::Device>& device, mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);
    ns::SharedPtr<mtl::RenderPipelineState> native;
    ns::SharedPtr<mtl::DepthStencilState>   depth;
};

struct MetalComputePipeline final : public ComputePipeline {
    MetalComputePipeline(mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& shader);
    ns::SharedPtr<mtl::ComputePipelineState> native;
};

class MetalSwapChain final : public SwapChain {
public:
    MetalSwapChain(const ns::SharedPtr<mtl::Device>& device, mtl4::CommandQueue& queue, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc);
    ~MetalSwapChain() final = default;
    auto AcquireTextureForRendering() -> utils::optional_ref<Texture> final;
    auto GetWidth() const noexcept -> std::uint32_t final;
    auto GetHeight() const noexcept -> std::uint32_t final;
    auto GetFormat() const noexcept -> Format final;
    void Present() final;
    void Resize() final;

private:
    const ns::SharedPtr<mtl::Device>&                       m_Device;
    mtl4::CommandQueue&                                     m_Queue;
    std::shared_ptr<spdlog::logger>                         m_Logger;
    SDL_Window*                                             m_Window = nullptr;
    std::unique_ptr<void, decltype(&SDL_Metal_DestroyView)> m_View{nullptr, SDL_Metal_DestroyView};
    // Native attachments must be released before the SDL view that owns the layer.
    ns::SharedPtr<ca::MetalLayer>    m_Layer;
    ns::SharedPtr<ca::MetalDrawable> m_Drawable;
    std::shared_ptr<MetalTexture>    m_Target;
    std::uint32_t                    m_Width = 0, m_Height = 0;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

// Keep the public Slang module identical on every backend. Only Metal lowers
// its SPIR-V descriptor arrays into a stable native argument-buffer layout.
auto compile_metal_source(const core::Buffer& bytecode) -> std::string {
    namespace cross = interop::cross;
    cross::CompilerMSL compiler(reinterpret_cast<const std::uint32_t*>(bytecode.GetData()), bytecode.GetDataSize() / 4);
    auto               options            = compiler.get_msl_options();
    options.msl_version                   = 40000;
    options.argument_buffers              = true;
    options.argument_buffers_tier         = decltype(options)::ArgumentBuffersTier::Tier2;
    options.enable_decoration_binding     = true;
    options.pad_argument_buffer_resources = true;
    compiler.set_msl_options(options);
    compiler.set_argument_buffer_device_address_space(0, true);
    const auto resources = compiler.get_shader_resources();
    for (const auto& resource : resources.storage_buffers) compiler.set_decoration(resource.id, cross::DecorationBinding, metal_buffer_binding);
    const auto remap_images = [&](const auto& images, bool storage) {
        for (const auto& resource : images) {
            const auto& type    = compiler.get_type(resource.base_type_id);
            const auto  scalar  = compiler.get_type(type.image.type).basetype;
            const auto  binding = type.image.dim == cross::Dim1D    ? (storage ? metal_storage_texture1d_binding : metal_texture1d_binding)
                                  : scalar == cross::SPIRType::UInt ? (storage ? metal_storage_uint_texture_binding : metal_uint_texture_binding)
                                  : scalar == cross::SPIRType::Int  ? (storage ? metal_storage_sint_texture_binding : metal_sint_texture_binding)
                                                                    : (storage ? metal_storage_texture_binding : metal_texture_binding);
            compiler.set_decoration(resource.id, cross::DecorationBinding, binding);
        }
    };
    remap_images(resources.separate_images, false);
    remap_images(resources.storage_images, true);
    for (const auto& resource : resources.separate_samplers) compiler.set_decoration(resource.id, cross::DecorationBinding, metal_sampler_binding);
    // Pad inactive regions as well: every stage and specialization shares the
    // CPU MetalBindlessUtils::ArgumentBufferLayout ABI, regardless of which descriptors survive DCE.
    for (auto index : {
             metal_buffer_binding,
             metal_texture1d_binding,
             metal_texture_binding,
             metal_uint_texture_binding,
             metal_sint_texture_binding,
             metal_storage_texture1d_binding,
             metal_storage_texture_binding,
             metal_storage_uint_texture_binding,
             metal_storage_sint_texture_binding,
             metal_sampler_binding,
         }) {
        cross::MSLResourceBinding binding;
        binding.stage      = compiler.get_execution_model();
        binding.desc_set   = 0;
        binding.binding    = index;
        binding.count      = index == metal_sampler_binding ? metal_sampler_capacity : metal_resource_capacity;
        binding.basetype   = index == metal_buffer_binding ? cross::SPIRType::UInt : index == metal_sampler_binding ? cross::SPIRType::Sampler
                                                                                                                    : cross::SPIRType::Image;
        binding.msl_buffer = binding.msl_texture = binding.msl_sampler = index;
        compiler.add_msl_resource_binding(binding);
    }
    // SPIRV-Cross returns std::string; preserve its allocation instead of copying into PMR.
    return compiler.compile();
}

MetalBuffer::MetalBuffer(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, GPUBufferDesc desc, std::span<const std::byte> initial_data)
    : GPUBuffer(std::move(desc)), native_device(device) {
    native = ns::TransferPtr(device->newBuffer(std::max(std::uint64_t{1}, Size()), mtl::ResourceStorageModeShared));
    check_metal(logger, native.get() != nullptr, "Create Metal buffer failed");
    native->setLabel(to_metal_string(m_Desc.name));
    if (!initial_data.empty()) {
        if (initial_data.size() > Size()) throw std::invalid_argument("Initial data exceeds Metal buffer");
        std::memcpy(native->contents(), initial_data.data(), initial_data.size());
    }
}

auto MetalBuffer::GetAllocationSize() const noexcept -> std::uint64_t { return native->allocatedSize(); }

auto MetalBuffer::Map() -> std::byte* { return static_cast<std::byte*>(native->contents()); }

void MetalBuffer::UnMap() {}

MetalBufferView::MetalBufferView(MetalBindlessUtils& bindings, GPUBufferViewDesc desc)
    : GPUBufferView(bindings, {.offset_alignment = 4, .size_alignment = 4}, std::move(desc)) {
    if (RequiresBindlessHandle()) {
        m_BindlessHandle = bindings.CreateBindlessHandle(dynamic_cast<MetalBuffer&>(*m_Desc.buffer).native.get(), m_Desc.offset, m_Desc.type == GPUBufferViewType::StorageWrite);
    }
}

MetalTexture::MetalTexture(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, TextureDesc desc, std::span<const std::byte> initial_data)
    : Texture(std::move(desc)), native_device(device) {
    auto texture_desc = ns::TransferPtr(mtl::TextureDescriptor::alloc()->init());
    texture_desc->setPixelFormat(to_metal_pixel_format(m_Desc.format));
    texture_desc->setWidth(m_Desc.width);
    texture_desc->setHeight(m_Desc.height);
    texture_desc->setDepth(m_Desc.depth);
    texture_desc->setArrayLength(m_Desc.array_size);
    texture_desc->setMipmapLevelCount(m_Desc.mip_levels);
    texture_desc->setTextureType(m_Desc.depth > 1 ? mtl::TextureType3D : m_Desc.array_size > 1 ? mtl::TextureType2DArray
                                                                                               : mtl::TextureType2D);
    auto usages = mtl::TextureUsageShaderRead | mtl::TextureUsagePixelFormatView;
    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::RenderTarget) || utils::has_flag(m_Desc.usages, TextureUsageFlags::DepthStencil)) usages |= mtl::TextureUsageRenderTarget;
    if (utils::has_flag(m_Desc.usages, TextureUsageFlags::UAV)) usages |= mtl::TextureUsageShaderWrite;
    texture_desc->setUsage(usages);
    texture_desc->setStorageMode(utils::has_flag(m_Desc.usages, TextureUsageFlags::DepthStencil) ? mtl::StorageModePrivate : mtl::StorageModeShared);
    native = ns::TransferPtr(device->newTexture(texture_desc.get()));
    check_metal(logger, native.get() != nullptr, "Create Metal texture failed");
    native->setLabel(to_metal_string(m_Desc.name));
    if (!initial_data.empty()) {
        std::size_t offset = 0;
        for (unsigned layer = 0; layer < m_Desc.array_size; ++layer)
            for (unsigned mip = 0; mip < m_Desc.mip_levels; ++mip) {
                auto width = std::max(1u, m_Desc.width >> mip), height = std::max(1u, m_Desc.height >> mip);
                auto row  = width * get_format_byte_size(m_Desc.format);
                auto size = std::size_t(row) * height;
                if (offset + size > initial_data.size()) throw std::invalid_argument("Insufficient initial Metal texture data");
                native->replaceRegion(mtl::Region::Make2D(0, 0, width, height), mip, layer, initial_data.data() + offset, row, size);
                offset += size;
            }
    }
}

MetalTexture::MetalTexture(const ns::SharedPtr<mtl::Device>& device, TextureDesc desc, mtl::Texture* texture) : Texture(std::move(desc)), native_device(device), native(ns::RetainPtr(texture)) {}

auto MetalTexture::GetAllocationSize() const noexcept -> std::uint64_t { return native->allocatedSize(); }

MetalTextureView::MetalTextureView(MetalBindlessUtils& bindings, const std::shared_ptr<spdlog::logger>& logger, TextureViewDesc desc) : TextureView(bindings, std::move(desc)) {
    auto& texture     = dynamic_cast<MetalTexture&>(*m_Desc.texture);
    auto  mip_count   = m_Desc.mip_levels ? m_Desc.mip_levels : texture.GetDesc().mip_levels - m_Desc.base_mip_level;
    auto  layer_count = m_Desc.layer_count ? m_Desc.layer_count : texture.GetDesc().array_size - m_Desc.base_array_layer;
    native            = ns::TransferPtr(texture.native->newTextureView(texture.native->pixelFormat(), texture.native->textureType(), ns::Range{m_Desc.base_mip_level, mip_count}, ns::Range{m_Desc.base_array_layer, layer_count}));
    check_metal(logger, native.get() != nullptr, "Create Metal texture view failed");
    if (RequiresBindlessHandle()) m_BindlessHandle = bindings.CreateBindlessHandle(native.get(), m_Desc.type == TextureViewType::ShaderWrite);
}

MetalSampler::MetalSampler(const ns::SharedPtr<mtl::Device>& device, const std::shared_ptr<spdlog::logger>& logger, MetalBindlessUtils& bindings, SamplerDesc desc) : Sampler(bindings, std::move(desc)) {
    auto sampler_desc = ns::TransferPtr(mtl::SamplerDescriptor::alloc()->init());
    auto address      = [](AddressMode mode) { return mode == AddressMode::Repeat ? mtl::SamplerAddressModeRepeat : mode == AddressMode::MirrorRepeat ? mtl::SamplerAddressModeMirrorRepeat
                                                                                                                                                      : mtl::SamplerAddressModeClampToEdge; };
    sampler_desc->setSAddressMode(address(m_Desc.address_u));
    sampler_desc->setTAddressMode(address(m_Desc.address_v));
    sampler_desc->setRAddressMode(address(m_Desc.address_w));
    sampler_desc->setMinFilter(m_Desc.min_filter == FilterMode::Linear ? mtl::SamplerMinMagFilterLinear : mtl::SamplerMinMagFilterNearest);
    sampler_desc->setMagFilter(m_Desc.mag_filter == FilterMode::Linear ? mtl::SamplerMinMagFilterLinear : mtl::SamplerMinMagFilterNearest);
    sampler_desc->setMipFilter(m_Desc.mipmap_filter == FilterMode::Linear ? mtl::SamplerMipFilterLinear : mtl::SamplerMipFilterNearest);
    sampler_desc->setLodMinClamp(m_Desc.min_lod);
    sampler_desc->setLodMaxClamp(m_Desc.max_lod);
    sampler_desc->setMaxAnisotropy(std::max(1u, static_cast<unsigned>(m_Desc.max_anisotropy)));
    sampler_desc->setSupportArgumentBuffers(true);
    native = ns::TransferPtr(device->newSamplerState(sampler_desc.get()));
    check_metal(logger, native.get() != nullptr, "Create Metal sampler failed");
    m_BindlessHandle = bindings.CreateBindlessHandle(native.get());
}

MetalShader::MetalShader(mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ShaderDesc desc, CompiledShader compiled) : Shader(std::move(desc), std::move(compiled)) {
    auto options = ns::TransferPtr(mtl::CompileOptions::alloc()->init());
    options->setLanguageVersion(mtl::LanguageVersion4_0);
    ns::Error* error        = nullptr;
    const auto source       = compile_metal_source(m_Compiled.bytecode);
    auto       library_desc = ns::TransferPtr(mtl4::LibraryDescriptor::alloc()->init());
    library_desc->setSource(to_metal_string(source));
    library_desc->setOptions(options.get());
    library_desc->setName(to_metal_string(m_Desc.name));
    library = ns::TransferPtr(compiler.newLibrary(library_desc.get(), &error));
    check_metal(logger, library.get() != nullptr, "Compile Slang Metal 4 shader", error);
    function = ns::TransferPtr(library->newFunction(to_metal_string(m_Compiled.entry)));
    if (!function && library->functionNames()->count() == 1) function = ns::TransferPtr(library->newFunction(static_cast<ns::String*>(library->functionNames()->object(0))));
    check_metal(logger, function.get() != nullptr, "Metal shader entry not found");
    descriptor = ns::TransferPtr(mtl4::LibraryFunctionDescriptor::alloc()->init());
    descriptor->setLibrary(library.get());
    descriptor->setName(function->name());
}

MetalRenderPipeline::MetalRenderPipeline(const ns::SharedPtr<mtl::Device>& device, mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) : RenderPipeline(std::move(desc)) {
    auto pipeline_desc = ns::TransferPtr(mtl4::RenderPipelineDescriptor::alloc()->init());
    for (const auto& shader : shaders) {
        auto& metal = dynamic_cast<MetalShader&>(*shader);
        if (shader->GetDesc().type == ShaderType::Vertex)
            pipeline_desc->setVertexFunctionDescriptor(metal.descriptor.get());
        else if (shader->GetDesc().type == ShaderType::Pixel)
            pipeline_desc->setFragmentFunctionDescriptor(metal.descriptor.get());
        else
            throw std::invalid_argument("Metal render pipeline supports vertex and pixel stages");
    }
    auto          vertex_desc = ns::TransferPtr(mtl::VertexDescriptor::alloc()->init());
    std::uint32_t ordinal     = 0;
    for (auto& attribute : m_Desc.vertex_input_layout) {
        auto location = attribute.location == std::numeric_limits<std::uint32_t>::max() ? ordinal : attribute.location;
        ++ordinal;
        if (location >= metal_max_buffer_bindings || attribute.binding + metal_vertex_buffer_slot >= metal_max_buffer_bindings) throw std::invalid_argument("Metal vertex attribute/buffer slot is out of range");
        auto* metal_attribute = vertex_desc->attributes()->object(location);
        metal_attribute->setFormat(to_metal_vertex_format(attribute.format));
        metal_attribute->setOffset(attribute.offset);
        metal_attribute->setBufferIndex(attribute.binding + metal_vertex_buffer_slot);
        auto* buffer_layout = vertex_desc->layouts()->object(attribute.binding + metal_vertex_buffer_slot);
        buffer_layout->setStride(attribute.stride ? attribute.stride : get_format_byte_size(attribute.format));
        buffer_layout->setStepFunction(attribute.per_instance ? mtl::VertexStepFunctionPerInstance : mtl::VertexStepFunctionPerVertex);
        buffer_layout->setStepRate(1);
    }
    pipeline_desc->setVertexDescriptor(vertex_desc.get());
    auto* color = pipeline_desc->colorAttachments()->object(0);
    color->setPixelFormat(to_metal_pixel_format(m_Desc.render_format));
    const auto& blend_state = m_Desc.blend_state;
    if (blend_state.logic_operation_enable) throw std::invalid_argument("Metal does not support logic operations");
    color->setBlendingState(blend_state.blend_enable ? mtl4::BlendStateEnabled : mtl4::BlendStateDisabled);
    color->setSourceRGBBlendFactor(to_metal_blend_factor(blend_state.src_color_blend_factor));
    color->setDestinationRGBBlendFactor(to_metal_blend_factor(blend_state.dst_color_blend_factor));
    color->setSourceAlphaBlendFactor(to_metal_blend_factor(blend_state.src_alpha_blend_factor));
    color->setDestinationAlphaBlendFactor(to_metal_blend_factor(blend_state.dst_alpha_blend_factor));
    color->setRgbBlendOperation(to_metal_blend_operation(blend_state.color_blend_op));
    color->setAlphaBlendOperation(to_metal_blend_operation(blend_state.alpha_blend_op));
    mtl::ColorWriteMask mask = mtl::ColorWriteMaskNone;
    if ((std::to_underlying(blend_state.color_write_mask) & std::to_underlying(ColorMask::R)) != 0) mask |= mtl::ColorWriteMaskRed;
    if ((std::to_underlying(blend_state.color_write_mask) & std::to_underlying(ColorMask::G)) != 0) mask |= mtl::ColorWriteMaskGreen;
    if ((std::to_underlying(blend_state.color_write_mask) & std::to_underlying(ColorMask::B)) != 0) mask |= mtl::ColorWriteMaskBlue;
    if ((std::to_underlying(blend_state.color_write_mask) & std::to_underlying(ColorMask::A)) != 0) mask |= mtl::ColorWriteMaskAlpha;
    color->setWriteMask(mask);
    // Metal 4 derives depth attachment format from the render pass.
    ns::Error* error = nullptr;
    native           = ns::TransferPtr(compiler.newRenderPipelineState(pipeline_desc.get(), nullptr, &error));
    check_metal(logger, native.get() != nullptr, "Create Metal render pipeline", error);
    auto        depth_desc          = ns::TransferPtr(mtl::DepthStencilDescriptor::alloc()->init());
    const auto& depth_stencil_state = m_Desc.depth_stencil_state;
    if (depth_stencil_state.stencil_test_enable || depth_stencil_state.depth_bounds_test_enable) throw std::invalid_argument("Metal stencil/depth-bounds pipeline state is not implemented");
    depth_desc->setDepthCompareFunction(depth_stencil_state.depth_test_enable ? to_metal_compare_function(depth_stencil_state.depth_compare_op) : mtl::CompareFunctionAlways);
    depth_desc->setDepthWriteEnabled(depth_stencil_state.depth_write_enable);
    depth = ns::TransferPtr(device->newDepthStencilState(depth_desc.get()));
    check_metal(logger, depth.get() != nullptr, "Create Metal depth state");
}

MetalComputePipeline::MetalComputePipeline(mtl4::Compiler& compiler, const std::shared_ptr<spdlog::logger>& logger, ComputePipelineDesc desc, const std::shared_ptr<Shader>& shader) : ComputePipeline(std::move(desc)) {
    ns::Error* error         = nullptr;
    auto       pipeline_desc = ns::TransferPtr(mtl4::ComputePipelineDescriptor::alloc()->init());
    pipeline_desc->setComputeFunctionDescriptor(dynamic_cast<MetalShader&>(*shader).descriptor.get());
    native = ns::TransferPtr(compiler.newComputePipelineState(pipeline_desc.get(), nullptr, &error));
    check_metal(logger, native.get() != nullptr, "Create Metal compute pipeline", error);
}

MetalSwapChain::MetalSwapChain(const ns::SharedPtr<mtl::Device>& device, mtl4::CommandQueue& queue, std::shared_ptr<spdlog::logger> logger, SwapChainDesc desc) : SwapChain(std::move(desc)), m_Device(device), m_Queue(queue), m_Logger(std::move(logger)) {
    if (m_Desc.window.type != utils::Window::Type::SDL3) throw std::invalid_argument("Metal requires an SDL window");
    m_Window = static_cast<SDL_Window*>(m_Desc.window.ptr);
    m_View.reset(SDL_Metal_CreateView(m_Window));
    check_metal(m_Logger, m_View != nullptr, SDL_GetError());
    m_Layer = ns::RetainPtr(static_cast<ca::MetalLayer*>(SDL_Metal_GetLayer(m_View.get())));
    check_metal(m_Logger, m_Layer.get() != nullptr, "SDL did not create a CAMetalLayer");
    m_Layer->setDevice(m_Device.get());
    m_Layer->setPixelFormat(mtl::PixelFormatBGRA8Unorm);
    m_Layer->setFramebufferOnly(false);
    m_Layer->setDisplaySyncEnabled(m_Desc.vsync);
    Resize();
}

auto MetalSwapChain::AcquireTextureForRendering() -> utils::optional_ref<Texture> {
    if (m_Drawable) return *m_Target;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_Window, &w, &h);
    if (w <= 0 || h <= 0) return {};
    if (std::cmp_not_equal(m_Width, w) || std::cmp_not_equal(m_Height, h)) Resize();
    m_Drawable = ns::RetainPtr(m_Layer->nextDrawable());
    if (!m_Drawable) return {};
    m_Queue.wait(m_Drawable.get());
    m_Target = std::allocate_shared<MetalTexture>(
        std::pmr::polymorphic_allocator<MetalTexture>{},
        m_Device,
        TextureDesc{
            .name        = m_Desc.name,
            .width       = m_Width,
            .height      = m_Height,
            .format      = Format::B8G8R8A8_UNORM,
            .clear_value = m_Desc.clear_color,
            .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
        },
        m_Drawable->texture());
    return *m_Target;
}

auto MetalSwapChain::GetWidth() const noexcept -> std::uint32_t { return m_Width; }

auto MetalSwapChain::GetHeight() const noexcept -> std::uint32_t { return m_Height; }

auto MetalSwapChain::GetFormat() const noexcept -> Format { return Format::B8G8R8A8_UNORM; }

void MetalSwapChain::Present() {
    if (!m_Drawable) return;
    m_Queue.signalDrawable(m_Drawable.get());
    m_Drawable->present();
    m_Drawable.reset();
}

void MetalSwapChain::Resize() {
    m_Drawable.reset();
    m_Target.reset();
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_Window, &w, &h);
    m_Width  = std::max(w, 0);
    m_Height = std::max(h, 0);
    m_Layer->setDrawableSize({.width = static_cast<double>(m_Width), .height = static_cast<double>(m_Height)});
}

}  // namespace hitagi::gfx
