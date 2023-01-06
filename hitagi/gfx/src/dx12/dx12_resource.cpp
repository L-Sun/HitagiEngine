#include "dx12_resource.hpp"
#include "dx12_device.hpp"
#include "utils.hpp"

namespace hitagi::gfx {

auto DX12SwapChain::GetBuffer(std::uint8_t index) -> Texture& {
    assert(swap_chain);
    if (back_buffers.empty()) {
        // it is important to reserve the capacity for no to reallocate,
        // the name in desc is always avaliable
        back_buffer_names.reserve(desc.frame_count);

        for (std::size_t i = 0; i < desc.frame_count; i++) {
            back_buffer_names.emplace_back(fmt::format("{}-buffer-{}", desc.name, i));

            ID3D12Resource* texture;
            ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&texture)));
            texture->SetName(std::pmr::wstring(back_buffer_names.back().begin(), back_buffer_names.back().end()).data());

            auto d3d_desc = texture->GetDesc();

            auto result = std::make_shared<DX12Texture>(
                device,
                Texture::Desc{
                    .name         = back_buffer_names.back(),
                    .width        = static_cast<std::uint32_t>(d3d_desc.Width),
                    .height       = static_cast<std::uint32_t>(d3d_desc.Height),
                    .depth        = d3d_desc.DepthOrArraySize,
                    .array_size   = d3d_desc.DepthOrArraySize,
                    .format       = desc.format,
                    .mip_levels   = d3d_desc.MipLevels,
                    .sample_count = d3d_desc.SampleDesc.Count,
                    .usages       = Texture::UsageFlags::RTV | Texture::UsageFlags::CopyDst | Texture::UsageFlags::CopySrc,
                });
            result->resource = texture;
            result->state    = D3D12_RESOURCE_STATE_COMMON;

            if (utils::has_flag(result->desc.usages, Texture::UsageFlags::RTV)) {
                auto rtv_desc = to_d3d_rtv_desc(result->desc);
                result->rtv   = static_cast<DX12Device&>(device).AllocateDescriptors(1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                static_cast<DX12Device&>(device).GetDevice()->CreateRenderTargetView(result->resource, &rtv_desc, result->rtv.cpu_handle);
            }
            back_buffers.emplace_back(std::move(result));
        }
    }

    return *back_buffers.at(index);
}

void DX12SwapChain::Resize() {
    associated_queue->WaitIdle();
    for (auto& back_buffer : back_buffers) {
        if (back_buffer->resource) back_buffer->resource->Release();
    }
    back_buffers.clear();

    HWND h_wnd = static_cast<HWND>(desc.window_ptr);
    RECT rect;
    GetClientRect(h_wnd, &rect);
    std::uint32_t width  = rect.right - rect.left;
    std::uint32_t height = rect.bottom - rect.top;

    UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (allow_tearing) {
        flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }
    ThrowIfFailed(swap_chain->ResizeBuffers(desc.frame_count, width, height, to_dxgi_format(desc.format), flags));
}

DX12SwapChain::~DX12SwapChain() {
    for (auto& back_buffer : back_buffers) {
        if (back_buffer->resource) back_buffer->resource->Release();
    }
}

}  // namespace hitagi::gfx