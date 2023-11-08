#pragma once
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/command_context.hpp>
#include <hitagi/gfx/command_queue.hpp>

namespace hitagi::gfx {

struct MockGPUBuffer : public GPUBuffer {
    MockGPUBuffer(Device& device, GPUBufferDesc desc) : GPUBuffer(device, std::move(desc)) {}

    auto Map() -> std::byte* final { return nullptr; }
    void UnMap() final {}

    core::Buffer buffer;
};

struct MockTexture : public Texture {
    MockTexture(Device& device, TextureDesc desc) : Texture(device, std::move(desc)) {}
};

struct MockSampler : public Sampler {
    MockSampler(Device& device, SamplerDesc desc) : Sampler(device, std::move(desc)) {}
};

struct MockSwapChain : public SwapChain {
    MockSwapChain(Device& device, SwapChainDesc desc) : SwapChain(device, std::move(desc)), texture(device, {}) {}

    auto AcquireTextureForRendering() -> Texture& final { return texture; }
    auto GetWidth() const noexcept -> std::uint32_t final { return 0; }
    auto GetHeight() const noexcept -> std::uint32_t final { return 0; }
    auto GetFormat() const noexcept -> Format final { return Format::UNKNOWN; }
    void Present() final {}
    void Resize() final {}

    MockTexture texture;
};

struct MockShader : public Shader {
    MockShader(Device& device, ShaderDesc desc) : Shader(device, std::move(desc)) {}
};

struct MockRenderPipeline : public RenderPipeline {
    MockRenderPipeline(Device& device, RenderPipelineDesc desc) : RenderPipeline(device, std::move(desc)) {}
};

struct MockComputePipeline : public ComputePipeline {
    MockComputePipeline(Device& device, ComputePipelineDesc desc) : ComputePipeline(device, std::move(desc)) {}
};

struct MockFence : public Fence {
    MockFence(Device& device, std::string_view name) : Fence(device, name) {}

    void Signal(std::uint64_t value) final {
        m_Value = value;
    }
    bool Wait(std::uint64_t value, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) final {
        return true;
    }
    auto GetCurrentValue() -> std::uint64_t final {
        return m_Value;
    }
    std::atomic_uint64_t m_Value;
};

struct MockBindlessUtils : public BindlessUtils {
    MockBindlessUtils(Device& device, std::string_view name) : BindlessUtils(device, name) {}

    auto CreateBindlessHandle(GPUBuffer& buffer, std::uint64_t index, bool writable = false) -> BindlessHandle final {
        return {
            .index    = counter++,
            .type     = BindlessHandleType::Buffer,
            .writable = writable,
            .version  = 0,
        };
    }
    auto CreateBindlessHandle(Texture& texture, bool writeable = false) -> BindlessHandle final {
        return {
            .index    = counter++,
            .type     = BindlessHandleType::Texture,
            .writable = writeable,
            .version  = 0,
        };
    }
    auto CreateBindlessHandle(Sampler& sampler) -> BindlessHandle final {
        return {
            .index    = counter++,
            .type     = BindlessHandleType::Sampler,
            .writable = false,
            .version  = 0,
        };
    }
    void          DiscardBindlessHandle(BindlessHandle handle) final {}
    std::uint32_t counter = 0;
};

struct MockCommandQueue : public CommandQueue {
    using CommandQueue::CommandQueue;

    void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) final {}
    void WaitIdle() final{};
};

struct MockGraphicsCommandContext : public GraphicsCommandContext {
    MockGraphicsCommandContext(Device& device, std::string_view name) : GraphicsCommandContext(device, name) {}

    void Begin() final {}
    void End() final {}
    void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) final {}

    void BeginRendering(Texture&                     render_target,
                        utils::optional_ref<Texture> depth_stencil,
                        bool                         clear_render_target = false,
                        bool                         clear_depth_stencil = false) final {}
    void EndRendering() final {}

    void SetPipeline(const RenderPipeline& pipeline) final {}

    void SetViewPort(const ViewPort& view_port) final {}
    void SetScissorRect(const Rect& scissor_rect) final {}
    void SetBlendColor(const math::vec4f& color) final {}

    void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0) final {}
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