module;

module gfx.base;
namespace hitagi::gfx {

auto GPUBuffer::GetStorageViewRequirements(const Device& device) noexcept -> StorageViewRequirements {
    return device.GetStorageBufferViewRequirements();
}

GPUBufferView::GPUBufferView(Device& device, GPUBufferViewDesc desc)
    : ResourceWithDesc(device, std::move(desc)) {
    const auto fail = [&](std::string_view reason) {
        throw std::invalid_argument(std::format("Invalid GPU buffer view({}): {}", GetName(), reason));
    };
    if (!m_Desc.buffer) fail("buffer is nullptr");
    if (&m_Desc.buffer->GetDevice() != &device) fail("buffer belongs to another device");
    if (m_Desc.element_size == 0) fail("element size must be larger than zero");
    if (m_Desc.element_stride == 0) m_Desc.element_stride = m_Desc.element_size;
    if (m_Desc.element_stride < m_Desc.element_size) fail("element stride is smaller than element size");

    const auto required_usage = m_Desc.type == GPUBufferViewType::StorageRead
                                    ? GPUBufferUsageFlags::StorageRead
                                    : GPUBufferUsageFlags::StorageWrite;
    const auto storage        = utils::has_flag(m_Desc.buffer->GetDesc().usages, required_usage);
    // Non-storage views also support typed mapping of vertex/index/copy buffers.
    const auto requirements = storage ? GPUBuffer::GetStorageViewRequirements(device)
                                      : GPUBuffer::StorageViewRequirements{.offset_alignment = 1, .size_alignment = 1};
    if (m_Desc.offset % requirements.offset_alignment != 0) fail("binding offset is not aligned");
    if (m_Desc.offset >= m_Desc.buffer->Size()) fail("offset is outside the buffer");

    const auto available = m_Desc.buffer->Size() - m_Desc.offset;
    const auto usable    = available - available % requirements.size_alignment;
    if (m_Desc.element_size > usable) fail("element exceeds the available binding range");
    const auto max_count = 1 + (usable - m_Desc.element_size) / m_Desc.element_stride;
    if (m_Desc.element_count == 0) m_Desc.element_count = max_count;
    if (m_Desc.element_count > max_count) fail("element range exceeds the buffer");

    // The preceding division check also prevents multiplication/addition overflow.
    const auto payload_size = (m_Desc.element_count - 1) * m_Desc.element_stride + m_Desc.element_size;
    m_ByteSize              = utils::align(payload_size, requirements.size_alignment);
    // Only the binding's tail is rounded; elements are never moved or padded implicitly.
}

auto GPUBuffer::Transition(BarrierAccess access, PipelineStage stage) -> GPUBufferBarrier {
    GPUBufferBarrier result{
        .src_access = m_CurrentAccess,
        .dst_access = access,
        .src_stage  = m_CurrentStage,
        .dst_stage  = stage,
        .buffer     = *this,
    };

    m_CurrentAccess = access;
    m_CurrentStage  = stage;

    return result;
}

auto Texture::Transition(BarrierAccess access, TextureLayout layout, PipelineStage stage) -> TextureBarrier {
    TextureBarrier result{
        .src_access = m_CurrentAccess,
        .dst_access = access,
        .src_stage  = m_CurrentStage,
        .dst_stage  = stage,
        .src_layout = m_CurrentLayout,
        .dst_layout = layout,
        .texture    = *this,
    };

    m_CurrentAccess = access;
    m_CurrentStage  = stage;
    m_CurrentLayout = layout;

    return result;
}

}  // namespace hitagi::gfx
