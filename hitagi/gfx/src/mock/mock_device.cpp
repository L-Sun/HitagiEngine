#include "mock_device.hpp"
#include "mock_resource.hpp"

namespace hitagi::gfx {
MockDevice::MockDevice(std::string_view name)
    : Device(Device::Type::Mock, name),
      m_Queues{
          std::make_shared<MockCommandQueue>(*this, CommandType::Graphics, "MockGraphicsQueue"),
          std::make_shared<MockCommandQueue>(*this, CommandType::Compute, "MockComputeQueue"),
          std::make_shared<MockCommandQueue>(*this, CommandType::Copy, "MockCopyQueue"),
      },
      m_BindlessUtils(std::make_shared<MockBindlessUtils>(*this, "MockBindless"))

{
}

void MockDevice::WaitIdle() {}

auto MockDevice::CreateFence(std::uint64_t initial_value, std::string_view name) -> std::shared_ptr<Fence> {
    return std::make_shared<MockFence>(*this, name);
}

auto MockDevice::GetCommandQueue(CommandType type) const -> CommandQueue& {
    return *m_Queues[type];
}

auto MockDevice::CreateCommandContext(CommandType type, std::string_view name) -> std::shared_ptr<CommandContext> {
    switch (type) {
        case CommandType::Graphics:
            return std::make_shared<MockGraphicsCommandContext>(*this, name);
        case CommandType::Compute:
            return std::make_shared<MockComputeCommandContext>(*this, name);
        case CommandType::Copy:
            return std::make_shared<MockCopyCommandContext>(*this, name);
        default:
            utils::unreachable();
    }
}

auto MockDevice::CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> {
    return std::make_shared<MockSwapChain>(*this, std::move(desc));
}

auto MockDevice::CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    return std::make_shared<MockGPUBuffer>(*this, std::move(desc));
}

auto MockDevice::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<MockTexture>(*this, std::move(desc));
}

auto MockDevice::CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<MockSampler>(*this, std::move(desc));
}

auto MockDevice::CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> {
    return std::make_shared<MockShader>(*this, std::move(desc));
}

auto MockDevice::CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<MockRenderPipeline>(*this, std::move(desc));
}

auto MockDevice::CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<MockComputePipeline>(*this, std::move(desc));
}

auto MockDevice::GetBindlessUtils() -> BindlessUtils& {
    return *m_BindlessUtils;
}

}  // namespace hitagi::gfx