module;
#include <tracy/Tracy.hpp>

module gfx;
import std;
import core;
import utils;
#ifdef _WIN32
import gfx.dx12;
#endif
import gfx.vulkan;
import gfx.mock;

namespace hitagi::gfx {

auto create_device(Device::Type type, std::string_view name) -> std::unique_ptr<Device> {
    ZoneScoped;

    switch (type) {
        case Device::Type::DX12:
#ifdef _WIN32
            return std::make_unique<DX12Device>(name);
#else
            return nullptr;
#endif
        case Device::Type::Vulkan:
            return std::make_unique<VulkanDevice>(name);
        case Device::Type::Mock:
            return std::make_unique<MockDevice>(name);
        default:
            return nullptr;
    }
}

auto readback_texture(Device& device, Texture& texture, TextureSubresourceLayer layer) -> core::Buffer {
    const auto& desc       = texture.GetDesc();
    const auto  pixel_size = get_format_byte_size(desc.format);
    const auto  width      = desc.width;
    const auto  height     = desc.height;
    const auto  depth      = static_cast<std::uint32_t>(desc.depth);

    auto readback_buffer = device.CreateGPUBuffer({
        .name          = "readback_buffer",
        .element_size  = pixel_size,
        .element_count = static_cast<std::uint64_t>(width) * height * depth,
        .usages        = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
    });

    device.WaitIdle();

    auto ctx = device.CreateCopyContext("readback_copy");
    ctx->Begin();
    ctx->ResourceBarrier(
        {}, {},
        {{texture.Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy)}});
    ctx->CopyTextureToBuffer(texture, {0, 0, 0}, {width, height, depth}, *readback_buffer, 0, layer);
    ctx->End();

    auto& queue = device.GetCommandQueue(CommandType::Copy);
    queue.Submit({{*ctx}});
    queue.WaitIdle();

    auto  mapped = readback_buffer->Map();
    auto  size   = readback_buffer->Size();
    core::Buffer result(size, mapped);
    readback_buffer->UnMap();

    return result;
}

}  // namespace hitagi::gfx
