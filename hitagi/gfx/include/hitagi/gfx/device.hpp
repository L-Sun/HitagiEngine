#pragma once
#include <hitagi/gfx/command_queue.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

namespace spdlog {
class logger;
}

namespace hitagi::gfx {
class Device {
public:
    enum struct Type : std::uint8_t {
        DX12,
        Vulkan,
        Mock
    } const device_type;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    virtual ~Device();

    static auto  Create(Type type, std::string_view name = "") -> std::unique_ptr<Device>;
    virtual void WaitIdle() = 0;

    virtual auto CreateFence(std::string_view name = "") -> std::shared_ptr<Fence>         = 0;
    virtual auto CreateSemaphore(std::string_view name = "") -> std::shared_ptr<Semaphore> = 0;

    virtual auto GetCommandQueue(CommandType type) const -> CommandQueue&                                     = 0;
    virtual auto CreateGraphicsContext(std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext> = 0;
    virtual auto CreateComputeContext(std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>   = 0;
    virtual auto CreateCopyContext(std::string_view name = "") -> std::shared_ptr<CopyCommandContext>         = 0;

    virtual auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain>                                               = 0;
    virtual auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> = 0;
    virtual auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture>       = 0;
    virtual auto CreatSampler(SamplerDesc desc) -> std::shared_ptr<Sampler>                                                      = 0;

    virtual auto CreateShader(ShaderDesc desc, std::span<const std::byte> binary_program = {}) -> std::shared_ptr<Shader> = 0;
    virtual auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline>                         = 0;
    virtual auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline>                      = 0;

    virtual void Profile(std::size_t frame_index) const = 0;

    inline auto GetLogger() const noexcept -> std::shared_ptr<spdlog::logger> { return m_Logger; }

protected:
    Device(Type type, std::string_view name);

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;
    std::function<void()>           report_debug_error_after_destroy_fn;
};
}  // namespace hitagi::gfx