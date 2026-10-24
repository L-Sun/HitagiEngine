module;
#include "interop/tracy_macros.hpp"

module gfx;
import interop.tracy;
import std;
import core;
import utils;
#ifdef _WIN32
import gfx.dx12;
#endif
import gfx.vulkan;
#ifdef __APPLE__
import gfx.metal;
#endif
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
        case Device::Type::Metal:
#ifdef __APPLE__
            return std::make_unique<MetalDevice>(name);
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

auto readback_texture(Device& device, CommandQueues& queues, BindlessUtils& bindings, Texture& texture, TextureSubresourceLayer layer) -> core::Buffer {
    const auto& desc       = texture.GetDesc();
    const auto  pixel_size = get_format_byte_size(desc.format);
    const auto  width      = desc.width;
    const auto  height     = desc.height;
    const auto  depth      = static_cast<std::uint32_t>(desc.depth);

    auto readback_buffer = hitagi::gfx::GPUBuffer::Create(device, {
                                                                      .name   = "readback_buffer",
                                                                      .size   = static_cast<std::uint64_t>(pixel_size) * width * height * depth,
                                                                      .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
                                                                  });

    queues.WaitIdle();

    if (texture.GetCurrentLayout() != TextureLayout::Common &&
        texture.GetCurrentLayout() != TextureLayout::Unkown) {
        auto gfx_ctx = hitagi::gfx::GraphicsCommandContext::Create(device, queues, bindings, "readback_transition");
        gfx_ctx->Begin();
        gfx_ctx->ResourceBarrier(
            {}, {},
            {{texture.Transition(BarrierAccess::None, TextureLayout::Common, PipelineStage::None)}});
        gfx_ctx->End();

        auto& gfx_queue = queues.Get(CommandType::Graphics);
        gfx_queue.Submit({{*gfx_ctx}});
        gfx_queue.WaitIdle();
    }

    auto ctx = hitagi::gfx::CopyCommandContext::Create(device, queues, "readback_copy");
    ctx->Begin();
    ctx->ResourceBarrier(
        {}, {},
        {{texture.Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy)}});
    ctx->CopyTextureToBuffer(texture, {0, 0, 0}, {width, height, depth}, *readback_buffer, 0, layer);
    // Release the texture before handing it back to a graphics/compute queue.
    // DX12 copy queues decay to Common after submission; keep tracking consistent.
    ctx->ResourceBarrier(
        {}, {},
        {{texture.Transition(BarrierAccess::None, TextureLayout::Common, PipelineStage::None)}});
    ctx->End();

    auto& queue = queues.Get(CommandType::Copy);
    queue.Submit({{*ctx}});
    queue.WaitIdle();

    auto  mapped = readback_buffer->Map();
    auto  size   = readback_buffer->Size();
    core::Buffer result(size, mapped);
    readback_buffer->UnMap();

    return result;
}

}  // namespace hitagi::gfx
