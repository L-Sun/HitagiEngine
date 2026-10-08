module gfx.mock;
namespace hitagi::gfx {

MockRenderPipeline::MockRenderPipeline(Device& device, RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>&)
    : RenderPipeline(device, std::move(desc)) {}

MockComputePipeline::MockComputePipeline(Device& device, ComputePipelineDesc desc, const std::shared_ptr<Shader>&)
    : ComputePipeline(device, std::move(desc)) {}

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

MockGPUBufferView::MockGPUBufferView(Device& device, GPUBufferViewDesc desc) : GPUBufferView(device, std::move(desc)) {
    CreateBindlessHandle();
}

MockTextureView::MockTextureView(Device& device, TextureViewDesc desc) : TextureView(device, std::move(desc)) {
    const auto fail = [&](std::string message) {
        throw std::invalid_argument(std::format("Invalid texture view({}): {}", GetName(), message));
    };

    if (!m_Desc.texture) {
        fail("texture is nullptr");
    }
    if (&m_Desc.texture->GetDevice() != &device) {
        fail("texture belongs to another device");
    }

    const auto& texture_desc = m_Desc.texture->GetDesc();
    const auto  single_subresource_view =
        m_Desc.type == TextureViewType::ShaderWrite ||
        m_Desc.type == TextureViewType::RenderTarget ||
        m_Desc.type == TextureViewType::DepthStencil;
    if (m_Desc.base_mip_level >= texture_desc.mip_levels) {
        fail("base mip level is outside the texture mip range");
    }
    if (m_Desc.mip_levels == 0) {
        m_Desc.mip_levels = single_subresource_view
                                ? 1
                                : texture_desc.mip_levels - m_Desc.base_mip_level;
    }
    if (m_Desc.base_mip_level + m_Desc.mip_levels > texture_desc.mip_levels) {
        fail("mip range exceeds texture mip levels");
    }
    if (single_subresource_view && m_Desc.mip_levels != 1) {
        fail("texture view type must reference exactly one mip level");
    }
    if (m_Desc.base_array_layer >= texture_desc.array_size) {
        fail("base array layer is outside the texture array range");
    }
    if (m_Desc.layer_count == 0) {
        m_Desc.layer_count = single_subresource_view
                                 ? 1
                                 : texture_desc.array_size - m_Desc.base_array_layer;
    }
    if (m_Desc.base_array_layer + m_Desc.layer_count > texture_desc.array_size) {
        fail("array layer range exceeds texture array size");
    }
    if (single_subresource_view && m_Desc.layer_count != 1) {
        fail("texture view type must reference exactly one array layer");
    }
    if (m_Desc.type == TextureViewType::ShaderRead && !utils::has_flag(texture_desc.usages, TextureUsageFlags::SRV)) {
        fail(std::format("shader read view requires texture usage {}", TextureUsageFlags::SRV));
    }
    if (m_Desc.type == TextureViewType::ShaderWrite && !utils::has_flag(texture_desc.usages, TextureUsageFlags::UAV)) {
        fail(std::format("shader write view requires texture usage {}", TextureUsageFlags::UAV));
    }
    if (m_Desc.type == TextureViewType::RenderTarget && !utils::has_flag(texture_desc.usages, TextureUsageFlags::RenderTarget)) {
        fail(std::format("render target view requires texture usage {}", TextureUsageFlags::RenderTarget));
    }
    if (m_Desc.type == TextureViewType::DepthStencil && !utils::has_flag(texture_desc.usages, TextureUsageFlags::DepthStencil)) {
        fail(std::format("depth stencil view requires texture usage {}", TextureUsageFlags::DepthStencil));
    }

    CreateBindlessHandle();
}

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

auto MockDevice::CreateGPUBufferView(GPUBufferViewDesc desc) -> std::shared_ptr<GPUBufferView> {
    return std::make_shared<MockGPUBufferView>(*this, std::move(desc));
}

auto MockDevice::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<MockTexture>(*this, std::move(desc));
}

auto MockDevice::CreateTextureView(TextureViewDesc desc) -> std::shared_ptr<TextureView> {
    return std::make_shared<MockTextureView>(*this, std::move(desc));
}

auto MockDevice::CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<MockSampler>(*this, std::move(desc));
}

auto MockDevice::CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> {
    return std::make_shared<MockShader>(*this, std::move(desc));
}

auto MockDevice::CreateRenderPipeline(RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<MockRenderPipeline>(*this, std::move(desc), shaders);
}

auto MockDevice::CreateComputePipeline(ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<MockComputePipeline>(*this, std::move(desc), cs);
}

auto MockDevice::GetBindlessUtils() -> BindlessUtils& {
    return *m_BindlessUtils;
}

}  // namespace hitagi::gfx
