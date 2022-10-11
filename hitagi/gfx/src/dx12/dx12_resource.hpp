#pragma once
#include "descriptor_heap.hpp"
#include "dx12_command_queue.hpp"
#include <hitagi/gfx/gpu_resource.hpp>

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

    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES  state;
};

template <typename T>
    requires std::derived_from<T, Resource>
struct DX12DescriptorWrapper : public T {
    using T::T;

    Descriptor cbv, uav, srv, rtv, dsv, sampler;
};

struct DX12GpuBufferView : public GpuBufferView {
    using GpuBufferView::GpuBufferView;

    std::optional<D3D12_VERTEX_BUFFER_VIEW> vbv;
    std::optional<D3D12_INDEX_BUFFER_VIEW>  ibv;
    Descriptor                              cbvs;
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
    inline auto GetCurrentBackBuffer() -> std::shared_ptr<Texture> final {
        return GetBuffer(GetCurrentBackIndex());
    }
    auto        GetBuffer(std::uint8_t index) -> std::shared_ptr<Texture> final;
    inline void Present() final {
        swap_chain->Present(0, 0);
        associated_queue->InsertFence();
    }

    ComPtr<IDXGISwapChain4>                                         swap_chain;
    std::pmr::vector<std::shared_ptr<DX12ResourceWrapper<Texture>>> back_buffers;
    std::pmr::vector<std::pmr::string>                              back_buffer_names;
    DX12CommandQueue*                                               associated_queue;
};

}  // namespace hitagi::gfx