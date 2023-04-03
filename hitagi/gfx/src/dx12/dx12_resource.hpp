#pragma once
#include "dx12_descriptor_heap.hpp"
#include <hitagi/gfx/gpu_resource.hpp>

#include <d3dx12/d3dx12.h>
#include <D3D12MemAlloc.h>
#include <wrl.h>
#include <dxgi1_6.h>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
class DX12Device;
class DX12SwapChain;

struct DX12GPUBuffer : GPUBuffer {
    DX12GPUBuffer(DX12Device& device, GPUBufferDesc desc, std::span<const std::byte> initial_data = {});

    auto GetMappedPtr() const noexcept -> std::byte* final;

    ComPtr<ID3D12Resource>      resource;
    ComPtr<D3D12MA::Allocation> allocation;
    std::size_t                 buffer_size = 0;
    std::byte*                  mapped_ptr  = nullptr;

    Descriptor cbvs, uavs;
};

struct DX12Texture : public Texture {
    DX12Texture(DX12Device& device, TextureDesc desc, std::span<const std::byte> initial_data = {});
    DX12Texture(DX12SwapChain& swap_chain, std::uint32_t index);

    ComPtr<ID3D12Resource>      resource;
    ComPtr<D3D12MA::Allocation> allocation;
    Descriptor                  srv, uav, rtv, dsv;
};

struct DX12Sampler : public Sampler {
    DX12Sampler(DX12Device& device, SamplerDesc desc);

    Descriptor sampler;
};

struct DX12Shader : public Shader {
    DX12Shader(DX12Device& device, ShaderDesc desc, std::span<const std::byte> binary_program = {});

    inline auto GetDXILData() const noexcept -> std::span<const std::byte> final {
        return binary_program.Span<const std::byte>();
    }
    inline auto GetShaderByteCode() const noexcept -> D3D12_SHADER_BYTECODE {
        return {binary_program.GetData(), binary_program.GetDataSize()};
    }

    core::Buffer binary_program;
};

struct DX12RenderPipeline : public RenderPipeline {
    DX12RenderPipeline(DX12Device& device, RenderPipelineDesc desc);

    ComPtr<ID3D12PipelineState> pipeline;
};

struct DX12ComputePipeline : public ComputePipeline {
    DX12ComputePipeline(DX12Device& device, ComputePipelineDesc desc);

    ComPtr<ID3D12PipelineState> pipeline;
};

class DX12SwapChain final : public SwapChain {
public:
    DX12SwapChain(DX12Device& device, SwapChainDesc desc);

    inline auto GetWidth() const noexcept -> std::uint32_t final { return m_D3D12Desc.Width; }
    inline auto GetHeight() const noexcept -> std::uint32_t final { return m_D3D12Desc.Height; }
    auto        GetFormat() const noexcept -> Format final;

    void Present() final;
    void Resize() final;

    auto AcquireTextureForRendering() -> DX12Texture&;

    inline auto GetDX12SwapChain() const noexcept { return m_SwapChain; }

private:
    ComPtr<IDXGISwapChain4> m_SwapChain;
    math::vec2u             m_Size;
    DXGI_SWAP_CHAIN_DESC1   m_D3D12Desc;

    std::pmr::vector<DX12Texture> m_BackBuffers;
};

}  // namespace hitagi::gfx