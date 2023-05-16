#pragma once
#include <hitagi/gfx/device.hpp>

namespace hitagi::gfx {
class MockDevice : public Device {
public:
    MockDevice(std::string_view name);
    void WaitIdle() final;

    auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> final;

    auto        GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto        CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> final;
    inline auto CreateGraphicsContext(std::string_view name = "") -> std::shared_ptr<GraphicsCommandContext>;
    inline auto CreateComputeContext(std::string_view name = "") -> std::shared_ptr<ComputeCommandContext>;
    inline auto CreateCopyContext(std::string_view name = "") -> std::shared_ptr<CopyCommandContext>;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreatSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc, std::span<const std::byte> binary_program = {}) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> final;

    auto GetBindlessUtils() -> BindlessUtils& final;

    void Profile(std::size_t frame_index) const final;

private:
    utils::EnumArray<std::shared_ptr<CommandQueue>, CommandType> m_Queues;
    std::shared_ptr<BindlessUtils>                               m_BindlessUtils;
};

}  // namespace hitagi::gfx