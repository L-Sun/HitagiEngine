#pragma once
#include <hitagi/gfx/command_queue.hpp>
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/bindless.hpp>
#include <hitagi/gfx/shader_compiler.hpp>
#include <hitagi/gfx/sync.hpp>

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

    static auto Create(Type type, std::string_view name = "") -> std::unique_ptr<Device>;

    virtual void WaitIdle() = 0;

    virtual auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> = 0;

    virtual auto GetCommandQueue(CommandType type) const -> CommandQueue&                                              = 0;
    virtual auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> = 0;
    inline auto  CreateGraphicsContext(std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext>;
    inline auto  CreateComputeContext(std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>;
    inline auto  CreateCopyContext(std::string_view name = "") -> std::shared_ptr<CopyCommandContext>;

    virtual auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain>                                               = 0;
    virtual auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> = 0;
    virtual auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture>       = 0;
    virtual auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler>                                                     = 0;

    virtual auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader>                            = 0;
    virtual auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline>    = 0;
    virtual auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> = 0;

    virtual auto GetBindlessUtils() -> BindlessUtils& = 0;

    virtual void Profile(std::size_t frame_index) const = 0;

    inline auto& GetShaderCompiler() const noexcept { return m_ShaderCompiler; }
    inline auto  GetLogger() const noexcept -> std::shared_ptr<spdlog::logger> { return m_Logger; }

protected:
    Device(Type type, std::string_view name);

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;
    ShaderCompiler                  m_ShaderCompiler;
    std::function<void()>           report_debug_error_after_destroy_fn;
};

inline auto Device::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::static_pointer_cast<GraphicsCommandContext>(CreateCommandContext(CommandType::Graphics, name));
};
inline auto Device::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::static_pointer_cast<ComputeCommandContext>(CreateCommandContext(CommandType::Compute, name));
};
inline auto Device::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return std::static_pointer_cast<CopyCommandContext>(CreateCommandContext(CommandType::Copy, name));
};

}  // namespace hitagi::gfx