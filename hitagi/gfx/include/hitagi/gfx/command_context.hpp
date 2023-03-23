#pragma once
#include <hitagi/gfx/sync.hpp>
#include <hitagi/utils/concepts.hpp>

#include <cstdint>

namespace hitagi::gfx {
class Device;

enum struct CommandType : std::uint8_t {
    Graphics,
    Compute,
    Copy
};

class CommandContext {
public:
    virtual ~CommandContext() = default;

    virtual void Begin() = 0;
    virtual void End()   = 0;
    virtual void Reset() = 0;

    virtual void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) = 0;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto  GetType() const noexcept { return m_Type; }

protected:
    CommandContext(Device& device, CommandType type, std::string_view name)
        : m_Device(device), m_Type(type), m_Name(name) {}

    Device&           m_Device;
    const CommandType m_Type;
    std::pmr::string  m_Name;
};

struct RenderingInfo {
    std::variant<std::reference_wrapper<Texture>, std::reference_wrapper<SwapChain>> render_target;
    utils::optional_ref<Texture>                                                     depth_stencil;
};

using BindlessHandle = std::uint32_t;

// This struct is used of push constant, which will refer to a GPUBuffer containing bindless handles
struct BindlessInfoOffset {
    BindlessHandle bindless_info_handle;
    std::uint32_t  user_data_1;
    std::uint32_t  user_data_2;
    std::uint32_t  user_data_3;
};

class GraphicsCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void BeginRendering(const RenderingInfo& info) = 0;
    virtual void EndRendering()                            = 0;

    virtual void SetPipeline(const RenderPipeline& pipeline) = 0;

    virtual void SetViewPort(const ViewPort& view_port)   = 0;
    virtual void SetScissorRect(const Rect& scissor_rect) = 0;
    virtual void SetBlendColor(const math::vec4f& color)  = 0;

    virtual void SetIndexBuffer(GPUBuffer& buffer)                     = 0;
    virtual void SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) = 0;

    virtual void PushBindlessInfo(const BindlessInfoOffset& info) = 0;

    virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0)                                     = 0;
    virtual void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) = 0;

    virtual void Present(SwapChain& swap_chain) = 0;

    virtual void CopyTexture(const Texture& src, Texture& dst) = 0;
};

class ComputeCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void SetPipeline(const ComputePipeline& pipeline) = 0;

    virtual void PushBindlessInfo(const BindlessInfoOffset& info) = 0;
};

class CopyCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) = 0;
    virtual void CopyTexture(const Texture& src, Texture& dst)                                                                      = 0;
    virtual void CopyBufferToTexture(
        const GPUBuffer& src,
        std::size_t      src_offset,
        Texture&         dst,
        math::vec3i      dst_offset,
        math::vec3u      extent,
        std::uint32_t    mip_level        = 0,
        std::uint32_t    base_array_layer = 0,
        std::uint32_t    layer_count      = 1) = 0;
};

}  // namespace hitagi::gfx