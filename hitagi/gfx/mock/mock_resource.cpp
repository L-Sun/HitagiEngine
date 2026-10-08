export module gfx.mock:resource;
import std;
import math;
import utils;
import core;
import gfx.base;

export namespace hitagi::gfx {

struct MockBindlessUtils : public BindlessUtils {
    MockBindlessUtils(const void* owner, std::string_view name) : BindlessUtils(name), m_Owner(owner) {}
    bool IsOwner(const void* owner) const noexcept { return m_Owner == owner; }

    auto CreateBindlessHandle(const SamplerDesc& desc) -> BindlessHandle {
        return {
            .index    = counter++,
            .type     = BindlessHandleType::Sampler,
            .writable = false,
            .version  = 0,
        };
    }
    void                 DiscardBindlessHandle(BindlessHandle handle) final {}
    std::atomic_uint32_t counter = 0;

private:
    const void* m_Owner;

public:
    auto CreateBindlessHandle(const GPUBufferViewDesc& desc) -> BindlessHandle {
        return BindlessHandle{
            .index    = counter++,
            .type     = BindlessHandleType::Buffer,
            .writable = desc.type == GPUBufferViewType::StorageWrite,
            .version  = 0,
        };
    }
    auto CreateBindlessHandle(const TextureViewDesc& desc) -> BindlessHandle {
        return BindlessHandle{
            .index    = counter++,
            .type     = BindlessHandleType::Texture,
            .writable = desc.type == TextureViewType::ShaderWrite,
            .version  = 0,
        };
    }
};

struct MockGPUBuffer : public GPUBuffer {
    MockGPUBuffer(const void* owner, GPUBufferDesc desc) : GPUBuffer(std::move(desc)), m_Owner(owner) {}

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return Size(); }
    auto Map() -> std::byte* final { return nullptr; }
    void UnMap() final {}

    core::Buffer buffer;

private:
    friend class GPUBufferView;
    friend class TextureView;
    const void* m_Owner;
};

struct MockGPUBufferView final : public GPUBufferView {
    MockGPUBufferView(BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc);
};

struct MockTexture : public Texture {
    MockTexture(const void* owner, TextureDesc desc) : Texture(std::move(desc)), m_Owner(owner) {}

    auto GetAllocationSize() const noexcept -> std::uint64_t final {
        auto       bytes      = std::uint64_t{0};
        auto       mip_width  = std::max(m_Desc.width, 1u);
        auto       mip_height = std::max(m_Desc.height, 1u);
        auto       mip_depth  = std::max<std::uint16_t>(m_Desc.depth, 1u);
        const auto bpp        = static_cast<std::uint64_t>(get_format_byte_size(m_Desc.format));

        for (std::uint16_t mip = 0; mip < std::max<std::uint16_t>(m_Desc.mip_levels, 1u); ++mip) {
            bytes += static_cast<std::uint64_t>(mip_width) *
                     static_cast<std::uint64_t>(mip_height) *
                     static_cast<std::uint64_t>(mip_depth) *
                     static_cast<std::uint64_t>(std::max<std::uint16_t>(m_Desc.array_size, 1u)) *
                     bpp;
            mip_width  = std::max(mip_width / 2u, 1u);
            mip_height = std::max(mip_height / 2u, 1u);
            mip_depth  = std::max<std::uint16_t>(mip_depth / 2u, 1u);
        }
        return bytes;
    }

private:
    friend class GPUBufferView;
    friend class TextureView;
    const void* m_Owner;
};

struct MockTextureView final : public TextureView {
    MockTextureView(BindlessUtils& bindings, TextureViewDesc desc);
};

struct MockSampler : public Sampler {
    MockSampler(BindlessUtils& bindings, SamplerDesc desc) : Sampler(bindings, std::move(desc)) {
        if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<MockBindlessUtils&>(bindings).CreateBindlessHandle(m_Desc);
    }
};

struct MockSwapChain : public SwapChain {
    MockSwapChain(const void* owner, SwapChainDesc desc) : SwapChain(std::move(desc)), texture(owner, {}) {}

    auto AcquireTextureForRendering() -> utils::optional_ref<Texture> final { return texture; }
    auto GetWidth() const noexcept -> std::uint32_t final { return width; }
    auto GetHeight() const noexcept -> std::uint32_t final { return height; }
    auto GetFormat() const noexcept -> Format final { return Format::UNKNOWN; }
    void Present() final {}
    void Resize() final {}

    MockTexture   texture;
    std::uint32_t width  = 1;
    std::uint32_t height = 1;
};

struct MockShader : public Shader {
    MockShader(ShaderDesc desc) : Shader(std::move(desc)) {}
};

struct MockRenderPipeline : public RenderPipeline {
    MockRenderPipeline(RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);
};

struct MockComputePipeline : public ComputePipeline {
    MockComputePipeline(ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs);
};

struct MockFence : public Fence {
    MockFence(std::string_view name) : Fence(name) {}

    void Signal(std::uint64_t value) final {
        m_Value = value;
    }
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = (std::chrono::milliseconds::max)()) final {
        return true;
    }
    auto GetCurrentValue() -> std::uint64_t final {
        return m_Value;
    }
    std::atomic_uint64_t m_Value;
};

struct MockCommandQueue : public CommandQueue {
    MockCommandQueue(const void* owner, CommandType type, std::string_view name) : CommandQueue(type, name), m_Owner(owner) {}
    bool IsOwner(const void* owner) const noexcept { return m_Owner == owner; }

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final {
        for (const auto& signal : signal_fences) {
            signal.fence.Signal(signal.value);
        }
    }
    void WaitIdle() final {};

private:
    const void* m_Owner;
};

struct MockGraphicsCommandContext : public GraphicsCommandContext {
    MockGraphicsCommandContext(std::string_view name) : GraphicsCommandContext(name) {}

    void Begin() final {}
    void End() final {}
    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final {}

    void BeginRendering(TextureView&                     render_target,
                        utils::optional_ref<TextureView> depth_stencil,
                        bool                             clear_render_target = false,
                        bool                             clear_depth_stencil = false) final {}
    void EndRendering() final {}

    void SetPipeline(const RenderPipeline& pipeline) final {}

    void SetViewPort(const ViewPort& view_port) final {}
    void SetScissorRect(const Rect& scissor_rect) final {}
    void SetBlendColor(const math::Color& color) final {}

    void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0, Format index_format = Format::R32_UINT) final {}
    void SetVertexBuffers(std::uint8_t                                             start_binding,
                          std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
                          std::span<const std::size_t>                             offset) final {}

    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final {}

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final {}
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final {}

    void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) final {}
};

struct MockComputeCommandContext : public ComputeCommandContext {
    MockComputeCommandContext(std::string_view name) : ComputeCommandContext(name) {}

    void Begin() final {}
    void End() final {}
    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final {}

    void SetPipeline(const ComputePipeline& pipeline) final {}

    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final {}
};

struct MockCopyCommandContext : public CopyCommandContext {
    MockCopyCommandContext(std::string_view name) : CopyCommandContext(name) {}

    void Begin() final {}
    void End() final {}
    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final {}

    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) final {}

    void CopyBufferToTexture(
        const GPUBuffer&        src,
        std::size_t             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer dst_layer = {}) final {}

    void CopyTextureToBuffer(
        const Texture&          src,
        math::vec3i             src_offset,
        math::vec3u             extent,
        GPUBuffer&              dst,
        std::size_t             dst_offset,
        TextureSubresourceLayer src_layer = {}) final {}

    void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) final {}
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

MockRenderPipeline::MockRenderPipeline(RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>&)
    : RenderPipeline(std::move(desc)) {}

MockComputePipeline::MockComputePipeline(ComputePipelineDesc desc, const std::shared_ptr<Shader>&)
    : ComputePipeline(std::move(desc)) {}

MockGPUBufferView::MockGPUBufferView(BindlessUtils& bindings, GPUBuffer::StorageViewRequirements storage_requirements, GPUBufferViewDesc desc)
    : GPUBufferView(bindings, storage_requirements, std::move(desc)) {
    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<MockBindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(m_Desc);
}

MockTextureView::MockTextureView(BindlessUtils& bindings, TextureViewDesc desc) : TextureView(bindings, std::move(desc)) {
    const auto fail = [&](std::string message) {
        throw std::invalid_argument(std::format("Invalid texture view({}): {}", GetName(), message));
    };

    if (!m_Desc.texture) {
        fail("texture is nullptr");
    }

    const auto& texture_desc = m_Desc.texture->GetDesc();
    const auto  single_subresource_view =
        m_Desc.type == TextureViewType::ShaderWrite ||
        m_Desc.type == TextureViewType::RenderTarget ||
        m_Desc.type == TextureViewType::DepthStencil;
    if (m_Desc.base_mip_level >= texture_desc.mip_levels) {
        fail("base mip level is outside the texture mip range");
    }
    if (m_Desc.mip_levels == 0) {
        m_Desc.mip_levels = single_subresource_view
                                ? 1
                                : texture_desc.mip_levels - m_Desc.base_mip_level;
    }
    if (m_Desc.base_mip_level + m_Desc.mip_levels > texture_desc.mip_levels) {
        fail("mip range exceeds texture mip levels");
    }
    if (single_subresource_view && m_Desc.mip_levels != 1) {
        fail("texture view type must reference exactly one mip level");
    }
    if (m_Desc.base_array_layer >= texture_desc.array_size) {
        fail("base array layer is outside the texture array range");
    }
    if (m_Desc.layer_count == 0) {
        m_Desc.layer_count = single_subresource_view
                                 ? 1
                                 : texture_desc.array_size - m_Desc.base_array_layer;
    }
    if (m_Desc.base_array_layer + m_Desc.layer_count > texture_desc.array_size) {
        fail("array layer range exceeds texture array size");
    }
    if (single_subresource_view && m_Desc.layer_count != 1) {
        fail("texture view type must reference exactly one array layer");
    }
    if (m_Desc.type == TextureViewType::ShaderRead && !utils::has_flag(texture_desc.usages, TextureUsageFlags::SRV)) {
        fail(std::format("shader read view requires texture usage {}", TextureUsageFlags::SRV));
    }
    if (m_Desc.type == TextureViewType::ShaderWrite && !utils::has_flag(texture_desc.usages, TextureUsageFlags::UAV)) {
        fail(std::format("shader write view requires texture usage {}", TextureUsageFlags::UAV));
    }
    if (m_Desc.type == TextureViewType::RenderTarget && !utils::has_flag(texture_desc.usages, TextureUsageFlags::RenderTarget)) {
        fail(std::format("render target view requires texture usage {}", TextureUsageFlags::RenderTarget));
    }
    if (m_Desc.type == TextureViewType::DepthStencil && !utils::has_flag(texture_desc.usages, TextureUsageFlags::DepthStencil)) {
        fail(std::format("depth stencil view requires texture usage {}", TextureUsageFlags::DepthStencil));
    }

    if (RequiresBindlessHandle()) m_BindlessHandle = static_cast<MockBindlessUtils&>(m_BindlessUtils).CreateBindlessHandle(m_Desc);
}

}  // namespace hitagi::gfx
