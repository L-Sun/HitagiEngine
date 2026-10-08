export module gfx.base:command_context;
import std;
import utils;
import math;
import :types;
import :gpu_resource;
import :sync;
import :bindless;

export namespace hitagi::gfx {

class CommandContext {
public:
    static auto Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext>;

    virtual ~CommandContext() = default;

    virtual void Begin() = 0;
    virtual void End()   = 0;

    virtual void ResourceBarrier(
        std::span<const GlobalBarrier>    global_barriers  = {},
        std::span<const GPUBufferBarrier> buffer_barriers  = {},
        std::span<const TextureBarrier>   texture_barriers = {}) = 0;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto GetType() const noexcept { return m_Type; }

protected:
    CommandContext(CommandType type, std::string_view name)
        : m_Type(type), m_Name(name) {}

    const CommandType m_Type;
    std::pmr::string  m_Name;
};

class GraphicsCommandContext : public CommandContext {
public:
    static auto Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext>;

    virtual void BeginRendering(TextureView&                     render_target,
                                utils::optional_ref<TextureView> depth_stencil       = {},
                                bool                             clear_render_target = false,
                                bool                             clear_depth_stencil = false) = 0;
    virtual void EndRendering()                                                               = 0;

    virtual void SetPipeline(const RenderPipeline& pipeline) = 0;

    virtual void SetViewPort(const ViewPort& view_port)   = 0;
    virtual void SetScissorRect(const Rect& scissor_rect) = 0;
    virtual void SetBlendColor(const math::Color& color)  = 0;

    virtual void SetIndexBuffer(const GPUBuffer& buffer, std::size_t offset = 0, Format index_format = Format::R32_UINT) = 0;
    virtual void SetVertexBuffers(
        std::uint8_t                                             start_binding,
        std::span<const std::reference_wrapper<const GPUBuffer>> buffers,
        std::span<const std::size_t>                             offsets) = 0;

    virtual void PushBindlessMetaInfo(const BindlessMetaInfo& info) = 0;

    virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0)                                     = 0;
    virtual void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) = 0;

    virtual void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) = 0;

protected:
    GraphicsCommandContext(std::string_view name) : CommandContext(CommandType::Graphics, name) {};
};

class ComputeCommandContext : public CommandContext {
public:
    static auto Create(Device& device, CommandQueues& queues, BindlessUtils& bindings, std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>;

    virtual void SetPipeline(const ComputePipeline& pipeline) = 0;

    virtual void PushBindlessMetaInfo(const BindlessMetaInfo& info) = 0;

protected:
    ComputeCommandContext(std::string_view name) : CommandContext(CommandType::Compute, name) {};
};

class CopyCommandContext : public CommandContext {
public:
    static auto Create(Device& device, CommandQueues& queues, std::string_view name = "") -> std::shared_ptr<CopyCommandContext>;

    virtual void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) = 0;
    virtual void CopyBufferToTexture(
        const GPUBuffer&        src,
        std::size_t             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer dst_layer = {}) = 0;

    virtual void CopyTextureToBuffer(
        const Texture&          src,
        math::vec3i             src_offset,
        math::vec3u             extent,
        GPUBuffer&              dst,
        std::size_t             dst_offset,
        TextureSubresourceLayer src_layer = {}) = 0;

    virtual void CopyTextureRegion(
        const Texture&          src,
        math::vec3i             src_offset,
        Texture&                dst,
        math::vec3i             dst_offset,
        math::vec3u             extent,
        TextureSubresourceLayer src_layer = {},
        TextureSubresourceLayer dst_layer = {}) = 0;

protected:
    CopyCommandContext(std::string_view name) : CommandContext(CommandType::Copy, name) {};
};

}  // namespace hitagi::gfx
