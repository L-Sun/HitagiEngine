#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::gfx {

GpuBuffer::GpuBuffer(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(desc) {
    m_Desc.name = m_Name;
}

Texture::Texture(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(desc) {
    m_Desc.name = m_Name;
}

Sampler::Sampler(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(desc) {
    m_Desc.name = m_Name;
}

SwapChain::SwapChain(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(desc) {
    m_Desc.name = m_Name;
}

RenderPipeline::RenderPipeline(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(std::move(desc)) {
    m_Desc.name = m_Name;
}

ComputePipeline::ComputePipeline(Device& device, Desc desc) : Resource(device, desc.name), m_Desc(std::move(desc)) {
    m_Desc.name = m_Name;
}

}  // namespace hitagi::gfx