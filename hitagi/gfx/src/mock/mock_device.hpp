#pragma once
#include <hitagi/gfx/device.hpp>

namespace hitagi::gfx {
class MockDevice : public Device {
public:
    MockDevice(std::string_view name);

    void Tick() final {}

    void WaitIdle() final;

    auto CreateFence(std::uint64_t initial_value = 0, std::string_view name = "") -> std::shared_ptr<Fence> final;

    auto GetCommandQueue(CommandType type) const -> CommandQueue& final;
    auto CreateCommandContext(CommandType type, std::string_view name = "") -> std::shared_ptr<CommandContext> final;

    auto CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> final;
    auto CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<GPUBuffer> final;
    auto CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data = {}) -> std::shared_ptr<Texture> final;
    auto CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> final;

    auto CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> final;
    auto CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> final;
    auto CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> final;

    auto GetBindlessUtils() -> BindlessUtils& final;

private:
    utils::EnumArray<std::shared_ptr<CommandQueue>, CommandType> m_Queues;
    std::shared_ptr<BindlessUtils>                               m_BindlessUtils;
};

}  // namespace hitagi::gfx