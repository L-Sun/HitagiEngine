#pragma once
#include "descriptor_heap.hpp"
#include "dx12_command_queue.hpp"

#include <hitagi/gfx/gpu_resource.hpp>

#include <D3D12MemAlloc.h>
#include <tracy/Tracy.hpp>

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

struct DX12GPUBuffer : public DX12ResourceWrapper<GPUBuffer> {
    using DX12ResourceWrapper<GPUBuffer>::DX12ResourceWrapper;

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

struct DX12SwapChain final : public SwapChain {
    using SwapChain::SwapChain;

    ~DX12SwapChain() final;

    auto GetCurrentBackBuffer() -> Texture& final;
    auto GetBuffers() -> std::pmr::vector<std::reference_wrapper<Texture>> final;

    inline void Present() final {
        swap_chain->Present(desc.vsync ? 1 : 0, allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        associated_queue->InsertFence();
    }
    auto Width() -> std::uint32_t final { return width; };
    auto Height() -> std::uint32_t final { return height; };
    void Resize() final;

    ComPtr<IDXGISwapChain4>                                         swap_chain;
    bool                                                            allow_tearing = false;
    std::pmr::vector<std::shared_ptr<DX12ResourceWrapper<Texture>>> back_buffers;
    std::pmr::vector<std::pmr::string>                              back_buffer_names;
    DX12CommandQueue*                                               associated_queue;
    std::uint32_t                                                   width, height;
};

}  // namespace hitagi::gfx