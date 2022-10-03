#pragma once
#include <cstddef>
#include <hitagi/gfx/command_queue.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#include <spdlog/logger.h>

namespace hitagi::gfx {
class Device {
public:
    enum struct Type : std::uint8_t {
        DX12,
        Vulkan,
        Mock
    } const device_type;

    virtual ~Device();

    static auto  Create(Type type, std::string_view name = "") -> std::unique_ptr<Device>;
    virtual void WaitIdle() = 0;

    virtual auto GetCommandQueue(CommandType type) const -> CommandQueue*                                          = 0;
    virtual auto CreateCommandQueue(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandQueue> = 0;
    virtual auto CreateGraphicsContext(std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext>      = 0;
    virtual auto CreateComputeContext(std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>        = 0;
    virtual auto CreateCopyContext(std::string_view name = "") -> std::shared_ptr<CopyCommandContext>              = 0;

    virtual auto CreateSwapChain(SwapChain::Desc desc) -> std::shared_ptr<SwapChain>                                            = 0;
    virtual auto CreateBuffer(GpuBuffer::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GpuBuffer> = 0;
    virtual auto CreateBufferView(GpuBufferView::Desc desc) -> std::shared_ptr<GpuBufferView>                                   = 0;
    virtual auto CreateTexture(Texture::Desc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture>    = 0;
    virtual auto CreateTextureView(TextureView::Desc desc) -> std::shared_ptr<TextureView>                                      = 0;
    virtual auto CreatSampler(Sampler::Desc desc) -> std::shared_ptr<Sampler>                                                   = 0;

    virtual auto CompileShader(Shader::Desc desc) -> std::shared_ptr<Shader>                        = 0;
    virtual auto CreateRenderPipeline(RenderPipeline::Desc desc) -> std::shared_ptr<RenderPipeline> = 0;

protected:
    Device(Type type, std::string_view name);

    std::shared_ptr<spdlog::logger> m_Logger;
    std::function<void()>           report_debug_error_after_destory_fn;
};
}  // namespace hitagi::gfx