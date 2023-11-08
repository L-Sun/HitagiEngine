#include <hitagi/render_graph/resource_node.hpp>
#include <hitagi/render_graph/render_graph.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::rg {

ResourceNode::ResourceNode(RenderGraph& render_graph, Type type, std::string_view name, std::shared_ptr<gfx::Resource> resource)
    : RenderGraphNode(render_graph, type, name), m_IsImported(resource != nullptr), m_Resource(std::move(resource)) {
}

void ResourceNode::InitializeMovedNode() {
    if (m_MoveTo == std::numeric_limits<std::size_t>::max()) return;

    const auto next_resource_node  = std::static_pointer_cast<ResourceNode>(m_RenderGraph->m_Nodes[m_MoveTo]);
    next_resource_node->m_Resource = m_Resource;
    next_resource_node->InitializeMovedNode();
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, gfx::GPUBufferDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::GPUBuffer, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> buffer, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::GPUBuffer, name, std::move(buffer)),
      m_Desc(std::static_pointer_cast<gfx::GPUBuffer>(m_Resource)->GetDesc()) {
}

auto GPUBufferNode::MoveTo(GPUBufferHandle handle, std::string_view name) -> GPUBufferNode {
    if (m_MoveTo != std::numeric_limits<std::size_t>::max()) {
        auto error_message = fmt::format("GPUBufferNode::MoveTo: already moved to {}", m_MoveTo);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    m_MoveTo = handle.index;
    if (m_IsImported) {
        GPUBufferNode new_node(*m_RenderGraph, std::static_pointer_cast<gfx::GPUBuffer>(m_Resource), name);
        return new_node;
    } else {
        GPUBufferNode new_node(*m_RenderGraph, m_Desc, name);
        return new_node;
    }
}

void GPUBufferNode::Initialize() {
    if (m_IsImported || m_Resource) return;
    m_Resource = m_RenderGraph->GetDevice().CreateGPUBuffer(m_Desc);
    InitializeMovedNode();
}

TextureNode::TextureNode(RenderGraph& render_graph, gfx::TextureDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Texture, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

TextureNode::TextureNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> texture, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Texture, name, std::move(texture)),
      m_Desc(std::static_pointer_cast<gfx::Texture>(m_Resource)->GetDesc()) {
}

auto TextureNode::MoveTo(TextureHandle handle, std::string_view name) -> TextureNode {
    if (m_MoveTo != std::numeric_limits<std::size_t>::max()) {
        auto error_message = fmt::format("TextureNode::MoveTo: already moved to {}", m_MoveTo);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }
    m_MoveTo = handle.index;
    if (m_IsImported) {
        TextureNode new_node(*m_RenderGraph, std::static_pointer_cast<gfx::Texture>(m_Resource), name);
        return new_node;
    } else {
        TextureNode new_node(*m_RenderGraph, m_Desc, name);
        return new_node;
    }
}

void TextureNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateTexture(m_Desc);
    InitializeMovedNode();
}

SamplerNode::SamplerNode(RenderGraph& render_graph, gfx::SamplerDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Sampler, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

SamplerNode::SamplerNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> sampler, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Sampler, name, std::move(sampler)),
      m_Desc(std::static_pointer_cast<gfx::Sampler>(m_Resource)->GetDesc()) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

void SamplerNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateSampler(m_Desc);
}

RenderPipelineNode::RenderPipelineNode(RenderGraph& render_graph, gfx::RenderPipelineDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::RenderPipeline, name), m_Desc(std::move(desc)) {}

RenderPipelineNode::RenderPipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::RenderPipeline, name, std::move(pipeline)),
      m_Desc(std::static_pointer_cast<gfx::RenderPipeline>(m_Resource)->GetDesc()) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

void RenderPipelineNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateRenderPipeline(m_Desc);
}

ComputePipelineNode::ComputePipelineNode(RenderGraph& render_graph, gfx::ComputePipelineDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::ComputePipeline, name), m_Desc(std::move(desc)) {}

ComputePipelineNode::ComputePipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::ComputePipeline, name, std::move(pipeline)),
      m_Desc(std::static_pointer_cast<gfx::ComputePipeline>(m_Resource)->GetDesc()) {
    if (m_Name.empty()) m_Name = m_Desc.name;
}

void ComputePipelineNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateComputePipeline(m_Desc);
}

}  // namespace hitagi::rg