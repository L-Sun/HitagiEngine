#pragma once
#include <hitagi/gfx/common_types.hpp>
#include <hitagi/gfx/gpu_resource.hpp>
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

    virtual void End() = 0;

    Device* const          device;
    const CommandType      type;
    const std::pmr::string name;

    std::uint64_t fence_value = 0;

protected:
    CommandContext(Device* device, CommandType type, std::string_view name)
        : device(device), type(type), name(name) {}
};

class GraphicsCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void SetViewPort(const ViewPort& view_port)                                                      = 0;
    virtual void SetScissorRect(const Rect& scissor_rect)                                                    = 0;
    virtual void SetBlendColor(const math::vec4f& color)                                                     = 0;
    virtual void SetRenderTarget(const TextureView& target)                                                  = 0;
    virtual void SetRenderTargetAndDepthStencil(const TextureView& target, const TextureView& depth_stencil) = 0;

    virtual void ClearRenderTarget(const TextureView& target)        = 0;
    virtual void ClearDepthStencil(const TextureView& depth_stencil) = 0;

    virtual void SetPipeline(const RenderPipeline& pipeline)                     = 0;
    virtual void SetIndexBuffer(const GpuBufferView& buffer)                     = 0;
    virtual void SetVertexBuffer(std::uint8_t slot, const GpuBufferView& buffer) = 0;

    template <typename T>
    requires utils::not_same_as<std::remove_cvref<T>, std::span<const std::byte>>
    void PushConstant(std::uint32_t slot, T&& data) {
        PushConstant(slot, {reinterpret_cast<const std::byte*>(&data), sizeof(T)});
    }
    virtual void PushConstant(std::uint32_t slot, const std::span<const std::byte>& data)                   = 0;
    virtual void BindConstantBuffer(std::uint32_t slot, const GpuBufferView& buffer, std::size_t index = 0) = 0;

    // Bindless resource
    virtual int GetBindless(const TextureView& texture_view) = 0;
    // virtual int GetBindless(const GpuBuffer& constant_buffer, std::size_t offset = 0, std::size_t size = 0) = 0;

    virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0)                                     = 0;
    virtual void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) = 0;

    virtual void Present(const TextureView& render_target) = 0;
};

class ComputeCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;
};

class CopyCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void CopyBuffer(const GpuBuffer& src, std::size_t src_offset, GpuBuffer& dest, std::size_t dest_offset, std::size_t size) = 0;
};

}  // namespace hitagi::gfx