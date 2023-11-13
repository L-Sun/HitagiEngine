#include <hitagi/render_graph/resource_node.hpp>
#include <hitagi/render_graph/render_graph.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::rg {

ResourceNode::ResourceNode(RenderGraph& render_graph, Type type, std::string_view name, std::shared_ptr<gfx::Resource> resource)
    : RenderGraphNode(render_graph, type, name), m_IsImported(resource != nullptr), m_Resource(std::move(resource)) {
}

auto ResourceNode::GetWriter() const noexcept -> PassNode* {
    for (auto* node : m_InputNodes) {
        if (node->IsPassNode()) return static_cast<PassNode*>(node);
    }
    return nullptr;
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, gfx::GPUBufferDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::GPUBuffer, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> buffer, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::GPUBuffer, name, std::move(buffer)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto GPUBufferNode::Move(GPUBufferHandle new_handle, std::string_view new_name) -> std::shared_ptr<GPUBufferNode> {
    if (m_MoveToNode != nullptr) {
        auto error_message = fmt::format("GPUBufferNode::MoveTo: already moved to {}", m_MoveToNode->GetName());
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::shared_ptr<GPUBufferNode> new_node;
    if (m_IsImported) {
        new_node = std::make_shared<GPUBufferNode>(*m_RenderGraph, std::static_pointer_cast<gfx::GPUBuffer>(m_Resource), new_name);
    } else {
        new_node = std::make_shared<GPUBufferNode>(*m_RenderGraph, GetDesc(), new_name);
    }

    new_node->AddInputNode(this);
    m_MoveToNode             = new_node.get();
    new_node->m_MoveFromNode = this;
    new_node->m_Handle       = new_handle.index;

    return new_node;
}

auto GPUBufferNode::GetDesc() const noexcept -> const gfx::GPUBufferDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::GPUBuffer>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

void GPUBufferNode::Initialize() {
    if (m_MoveFromNode) {
        if (m_MoveFromNode->m_Resource == nullptr) m_MoveFromNode->Initialize();
        m_Resource = m_MoveFromNode->m_Resource;
    }
    if (m_IsImported || m_Resource) return;
    m_Resource = m_RenderGraph->GetDevice().CreateGPUBuffer(m_Desc.value());
}

TextureNode::TextureNode(RenderGraph& render_graph, gfx::TextureDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Texture, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = m_Desc->name;
}

TextureNode::TextureNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> texture, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Texture, name, std::move(texture)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto TextureNode::Move(TextureHandle new_handle, std::string_view new_name) -> std::shared_ptr<TextureNode> {
    if (m_MoveToNode != nullptr) {
        auto error_message = fmt::format("TextureNode::MoveTo: already moved to {}", m_MoveToNode->GetName());
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::shared_ptr<TextureNode> new_node;
    if (m_IsImported) {
        new_node = std::make_shared<TextureNode>(*m_RenderGraph, std::static_pointer_cast<gfx::Texture>(m_Resource), new_name);
    } else {
        new_node = std::make_shared<TextureNode>(*m_RenderGraph, GetDesc(), new_name);
    }

    new_node->AddInputNode(this);
    m_MoveToNode             = new_node.get();
    new_node->m_MoveFromNode = this;
    new_node->m_Handle       = new_handle.index;

    return new_node;
}

auto TextureNode::GetDesc() const noexcept -> const gfx::TextureDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::Texture>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

void TextureNode::Initialize() {
    if (m_MoveFromNode) {
        if (m_MoveFromNode->m_Resource == nullptr) m_MoveFromNode->Initialize();
        m_Resource = m_MoveFromNode->m_Resource;
    }
    if (m_IsImported || m_Resource) return;
    m_Resource = m_RenderGraph->GetDevice().CreateTexture(m_Desc.value());
}

SamplerNode::SamplerNode(RenderGraph& render_graph, gfx::SamplerDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Sampler, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

SamplerNode::SamplerNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> sampler, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Sampler, name, std::move(sampler)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto SamplerNode::GetDesc() const noexcept -> const gfx::SamplerDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::Sampler>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

void SamplerNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateSampler(m_Desc.value());
}

RenderPipelineNode::RenderPipelineNode(RenderGraph& render_graph, gfx::RenderPipelineDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::RenderPipeline, name), m_Desc(std::move(desc)) {}

RenderPipelineNode::RenderPipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::RenderPipeline, name, std::move(pipeline)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto RenderPipelineNode::GetDesc() const noexcept -> const gfx::RenderPipelineDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::RenderPipeline>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

void RenderPipelineNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateRenderPipeline(m_Desc.value());
}

ComputePipelineNode::ComputePipelineNode(RenderGraph& render_graph, gfx::ComputePipelineDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::ComputePipeline, name), m_Desc(std::move(desc)) {}

ComputePipelineNode::ComputePipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::ComputePipeline, name, std::move(pipeline)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto ComputePipelineNode::GetDesc() const noexcept -> const gfx::ComputePipelineDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::ComputePipeline>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

void ComputePipelineNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = m_RenderGraph->GetDevice().CreateComputePipeline(m_Desc.value());
}

}  // namespace hitagi::rg