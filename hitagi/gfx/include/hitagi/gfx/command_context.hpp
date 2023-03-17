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
    // When a resource will be used on other type command queue, you should reset the resource state
    virtual void ResetState(GPUBuffer& buffer) = 0;
    virtual void ResetState(Texture& texture)  = 0;

    virtual void Begin() = 0;
    virtual void End()   = 0;

    virtual void Reset() = 0;

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

class GraphicsCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void SetViewPort(const ViewPort& view_port)                                  = 0;
    virtual void SetScissorRect(const Rect& scissor_rect)                                = 0;
    virtual void SetBlendColor(const math::vec4f& color)                                 = 0;
    virtual void SetRenderTarget(Texture& target)                                        = 0;
    virtual void SetRenderTargetAndDepthStencil(Texture& target, Texture& depth_stencil) = 0;

    virtual void ClearRenderTarget(Texture& target)        = 0;
    virtual void ClearDepthStencil(Texture& depth_stencil) = 0;

    virtual void SetPipeline(const GraphicsPipeline& pipeline)         = 0;
    virtual void SetIndexBuffer(GPUBuffer& buffer)                     = 0;
    virtual void SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) = 0;

    template <typename T>
        requires utils::not_same_as<std::remove_cvref<T>, std::span<const std::byte>>
    void PushConstant(std::uint32_t slot, T&& data) {
        PushConstant(slot, {reinterpret_cast<const std::byte*>(&data), sizeof(T)});
    }
    virtual void PushConstant(std::uint32_t slot, const std::span<const std::byte>& data)         = 0;
    virtual void BindConstantBuffer(std::uint32_t slot, GPUBuffer& buffer, std::size_t index = 0) = 0;
    virtual void BindTexture(std::uint32_t slot, Texture& texture)                                = 0;

    // Bindless resource
    virtual int GetBindless(const Texture& texture) = 0;
    // virtual int GetBindless(const GPUBuffer& constant_buffer, std::size_t offset = 0, std::size_t size = 0) = 0;

    virtual void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0)                                     = 0;
    virtual void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) = 0;

    virtual void CopyTexture(const Texture& src, Texture& dest) = 0;
};

class ComputeCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;
};

class CopyCommandContext : public CommandContext {
public:
    using CommandContext::CommandContext;

    virtual void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dest, std::size_t dest_offset, std::size_t size) = 0;
    virtual void CopyTexture(const Texture& src, const Texture& dest)                                                                 = 0;
};

}  // namespace hitagi::gfx