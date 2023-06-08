#pragma once
#include <hitagi/gfx/command_context.hpp>

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {

class DX12Device;

class DX12GraphicsCommandList : public GraphicsCommandContext {
public:
    DX12GraphicsCommandList(DX12Device& device, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void BeginRendering(Texture& render_target, utils::optional_ref<Texture> depth_stencil = {}) final;
    void EndRendering() final;

    void SetPipeline(const RenderPipeline& pipeline) final;

    void SetViewPort(const ViewPort& view_port) final;
    void SetScissorRect(const Rect& scissor_rect) final;
    void SetBlendColor(const math::vec4f& color) final;

    void SetIndexBuffer(GPUBuffer& buffer) final;
    void SetVertexBuffer(std::uint8_t slot, GPUBuffer& buffer) final;

    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    void Draw(std::uint32_t vertex_count, std::uint32_t instance_count = 1, std::uint32_t first_vertex = 0, std::uint32_t first_instance = 0) final;
    void DrawIndexed(std::uint32_t index_count, std::uint32_t instance_count = 1, std::uint32_t first_index = 0, std::uint32_t base_vertex = 0, std::uint32_t first_instance = 0) final;

    void CopyTexture(const Texture& src, Texture& dest) final;

    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator>    command_allocator;
    const RenderPipeline*             m_Pipeline = nullptr;
};

class DX12ComputeCommandList : public ComputeCommandContext {
public:
    DX12ComputeCommandList(DX12Device& device, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void SetPipeline(const ComputePipeline& pipeline) final;
    void PushBindlessMetaInfo(const BindlessMetaInfo& info) final;

    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator>    command_allocator;
    const ComputePipeline*            m_Pipeline = nullptr;
};

class DX12CopyCommandList : public CopyCommandContext {
public:
    DX12CopyCommandList(DX12Device& device, std::string_view name);
    void Begin() final;
    void End() final;

    void ResourceBarrier(
        const std::pmr::vector<GlobalBarrier>&    global_barriers  = {},
        const std::pmr::vector<GPUBufferBarrier>& buffer_barriers  = {},
        const std::pmr::vector<TextureBarrier>&   texture_barriers = {}) final;

    void CopyBuffer(const GPUBuffer& src, std::size_t src_offset, GPUBuffer& dst, std::size_t dst_offset, std::size_t size) final;
    void CopyTexture(const Texture& src, Texture& dst) final;
    void CopyBufferToTexture(
        const GPUBuffer& src,
        std::size_t      src_offset,
        Texture&         dst,
        math::vec3i      dst_offset,
        math::vec3u      extent,
        std::uint32_t    mip_level        = 0,
        std::uint32_t    base_array_layer = 0,
        std::uint32_t    layer_count      = 1) final;

    ComPtr<ID3D12GraphicsCommandList> command_list;
    ComPtr<ID3D12CommandAllocator>    command_allocator;
};

}  // namespace hitagi::gfx