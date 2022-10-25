#pragma once
#include "descriptor_heap.hpp"
#include "dx12_command_queue.hpp"
#include <hitagi/gfx/gpu_resource.hpp>

#include <D3D12MemAlloc.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>
#include <concepts>

using namespace Microsoft::WRL;

namespace hitagi::gfx {
template <typename T>
    requires std::derived_from<T, Resource>
struct DX12ResourceWrapper : public T {
    using T::T;

    ComPtr<D3D12MA::Allocation> allocation;
    ID3D12Resource*             resource;  // a reference to the allocation
    D3D12_RESOURCE_STATES       state;
};

struct DX12Texture : public DX12ResourceWrapper<Texture> {
    using DX12ResourceWrapper<Texture>::DX12ResourceWrapper;
    Descriptor uav, srv, rtv, dsv;
};

struct DX12GpuBuffer : public DX12ResourceWrapper<GpuBuffer> {
    using DX12ResourceWrapper<GpuBuffer>::DX12ResourceWrapper;

    std::optional<D3D12_VERTEX_BUFFER_VIEW> vbv;
    std::optional<D3D12_INDEX_BUFFER_VIEW>  ibv;
    Descriptor                              cbvs;
};

struct DX12Sampler : public Sampler {
    using Sampler::Sampler;
    Descriptor sampler;
};

struct DX12RenderPipeline : public RenderPipeline {
    using RenderPipeline::RenderPipeline;

    ComPtr<ID3D12VersionedRootSignatureDeserializer> root_signature_deserializer = nullptr;
    D3D12_ROOT_SIGNATURE_DESC1                       root_signature_desc;
    ComPtr<ID3D12RootSignature>                      root_signature = nullptr;
    ComPtr<ID3D12PipelineState>                      pso            = nullptr;
};

struct DX12SwapChain : public SwapChain {
    using SwapChain::SwapChain;

    inline auto GetCurrentBackIndex() -> std::uint8_t final {
        return swap_chain->GetCurrentBackBufferIndex();
    }
    inline auto GetCurrentBackBuffer() -> Texture& final {
        return GetBuffer(GetCurrentBackIndex());
    }
    auto        GetBuffer(std::uint8_t index) -> Texture& final;
    inline void Present() final {
        swap_chain->Present(desc.vsync ? 1 : 0, allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        associated_queue->InsertFence();
    }
    void Resize() final;

    ComPtr<IDXGISwapChain4>                                         swap_chain;
    bool                                                            allow_tearing = false;
    std::pmr::vector<std::shared_ptr<DX12ResourceWrapper<Texture>>> back_buffers;
    std::pmr::vector<std::pmr::string>                              back_buffer_names;
    DX12CommandQueue*                                               associated_queue;
};

}  // namespace hitagi::gfx