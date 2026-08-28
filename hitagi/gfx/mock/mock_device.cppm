export module gfx.mock;
import std;
import math;
import utils;
import core;
import gfx.base;

export namespace hitagi::gfx {

struct MockGPUBuffer : public GPUBuffer {
    MockGPUBuffer(Device& device, GPUBufferDesc desc) : GPUBuffer(device, std::move(desc)) {}

    auto GetAllocationSize() const noexcept -> std::uint64_t final { return Size(); }
    auto Map() -> std::byte* final { return nullptr; }
    void UnMap() final {}

    core::Buffer buffer;
};

struct MockGPUBufferView final : public GPUBufferView {
    MockGPUBufferView(Device& device, GPUBufferViewDesc desc);
};

struct MockTexture : public Texture {
    MockTexture(Device& device, TextureDesc desc) : Texture(device, std::move(desc)) {}

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
};

struct MockTextureView final : public TextureView {
    MockTextureView(Device& device, TextureViewDesc desc);
};

struct MockSampler : public Sampler {
    MockSampler(Device& device, SamplerDesc desc) : Sampler(device, std::move(desc)) {}
};

struct MockSwapChain : public SwapChain {
    MockSwapChain(Device& device, SwapChainDesc desc) : SwapChain(device, std::move(desc)), texture(device, {}) {}

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
    MockShader(Device& device, ShaderDesc desc) : Shader(device, std::move(desc)) {}
};

struct MockRenderPipeline : public RenderPipeline {
    MockRenderPipeline(Device& device, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders);
};

struct MockComputePipeline : public ComputePipeline {
    MockComputePipeline(Device& device, ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs);
};

struct MockFence : public Fence {
    MockFence(Device& device, std::string_view name) : Fence(device, name) {}

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

struct MockBindlessUtils : public BindlessUtils {
    MockBindlessUtils(Device& device, std::string_view name) : BindlessUtils(device, name) {}

    auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle final {
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
    auto CreateBindlessHandle(GPUBufferView& view) -> BindlessHandle final {
        return BindlessHandle{
            .index    = counter++,
            .type     = BindlessHandleType::Buffer,
            .writable = view.GetDesc().type == GPUBufferViewType::StorageWrite,
            .version  = 0,
        };
    }
    auto CreateBindlessHandle(TextureView& view) -> BindlessHandle final {
        return BindlessHandle{
            .index    = counter++,
            .type     = BindlessHandleType::Texture,
            .writable = view.GetDesc().type == TextureViewType::ShaderWrite,
            .version  = 0,
        };
    }
};

struct MockCommandQueue : public CommandQueue {
    using CommandQueue::CommandQueue;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final {
        for (const auto& signal : signal_fences) {
            signal.fence.Signal(signal.value);
        }
    }
    void WaitIdle() final {};
};

struct MockGraphicsCommandContext : public GraphicsCommandContext {
    MockGraphicsCommandContext(Device& device, std::string_view name) : GraphicsCommandContext(device, name) {}

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
    MockComputeCommandContext(Device& device, std::string_view name) : ComputeCommandContext(device, name) {}

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
    MockCopyCommandContext(Device& device, std::string_view name) : CopyCommandContext(device, name) {}

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

class MockDevice : public Device {
public:
    MockDevice(std::string_view name);

    void Tick() final {}

    void WaitIdle() final;

    auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> final;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateGPUBufferView(GPUBufferViewDesc desc) -> std::shared_ptr<GPUBufferView> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreateTextureView(TextureViewDesc desc) -> std::shared_ptr<TextureView> final;
    auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) -> std::shared_ptr<ComputePipeline> final;

    auto GetBindlessUtils() -> BindlessUtils& final;

private:
    utils::EnumArray<std::shared_ptr<CommandQueue>, CommandType> m_Queues;
    std::shared_ptr<BindlessUtils>                               m_BindlessUtils;
};

}  // namespace hitagi::gfx
