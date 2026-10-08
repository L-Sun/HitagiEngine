module;
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>
module gfx.render_graph;
namespace hitagi::rg {

PassNode::PassNode(RenderGraph& graph, Type type) : RenderGraphNode(graph, type) {
    static std::uint64_t next_owner{1};
    m_AccessOwner = next_owner++;
}

auto PassNode::Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer& {
    if (!m_RenderGraph->IsValid(buffer)) {
        std::string error_message = std::format("Buffer({}) is not valid handle", buffer.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph->m_Nodes[buffer.index].get());

    if (!std::ranges::any_of(m_GPUBufferEdges, [buffer_node](const auto& edge) { return edge.resource == buffer_node; })) {
        std::string error_message = std::format("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return buffer_node->Resolve();
}

auto PassNode::Resolve(TextureHandle texture) const -> gfx::Texture& {
    if (!m_RenderGraph->IsValid(texture)) {
        std::string error_message = std::format("Texture({}) is not valid handle", texture.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto texture_node = static_cast<TextureNode*>(m_RenderGraph->m_Nodes[texture.index].get());

    if (!std::ranges::any_of(m_TextureEdges, [texture_node](const auto& edge) { return edge.resource == texture_node; })) {
        std::string error_message = std::format("Texture({}) is not used in pass({})", texture.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return texture_node->Resolve();
}

auto PassNode::Resolve(SamplerHandle sampler) const -> gfx::Sampler& {
    if (!m_RenderGraph->IsValid(sampler)) {
        std::string error_message = std::format("Sampler({}) is not valid handle", sampler.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph->m_Nodes[sampler.index].get());

    if (!m_Samplers.contains(sampler_node)) {
        std::string error_message = std::format("Sampler({}) is not used in pass({})", sampler.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return sampler_node->Resolve();
}

auto PassNode::Resolve(GPUBufferEdgeHandle handle) const -> gfx::GPUBufferView& {
    if (handle.owner != m_AccessOwner || handle.index >= m_GPUBufferEdges.size()) {
        throw std::out_of_range("Access handle does not belong to this pass");
    }
    const auto& view = m_GPUBufferEdges[handle.index].view;
    if (!view) throw std::logic_error("Access has no view available");
    return *view;
}

auto PassNode::Resolve(TextureEdgeHandle handle) const -> gfx::TextureView& {
    if (handle.owner != m_AccessOwner || handle.index >= m_TextureEdges.size()) {
        throw std::out_of_range("Access handle does not belong to this pass");
    }
    const auto& view = m_TextureEdges[handle.index].view;
    if (!view) throw std::logic_error("Access has no view available");
    return *view;
}

auto PassNode::GetCommandType() const noexcept -> gfx::CommandType {
    switch (m_Type) {
        case Type::RenderPass:
        case Type::PresentPass:
            return gfx::CommandType::Graphics;
        case Type::ComputePass:
            return gfx::CommandType::Compute;
        case Type::CopyPass:
            return gfx::CommandType::Copy;
        default:
            utils::unreachable();
    }
}

void PassNode::Initialize() {
    auto& device = m_RenderGraph->GetDevice();

    if (m_CommandContext == nullptr)
        m_CommandContext = device.CreateCommandContext(GetCommandType(), GetName());

    // Views are prepared immediately before recording each pass.
}

void PassNode::PrepareResourceBarriers() {
    auto& device = m_RenderGraph->GetDevice();

    m_GPUBufferBarriers.clear();
    m_TextureBarriers.clear();

    // Aggregate by physical resource: multiple access views do not imply independent barriers.
    struct BufferAccess {
        gfx::GPUBuffer*    resource;
        bool               write;
        gfx::BarrierAccess access;
        gfx::PipelineStage stage;
    };
    struct TextureAccess {
        gfx::Texture*      resource;
        bool               write;
        gfx::BarrierAccess access;
        gfx::PipelineStage stage;
        gfx::TextureLayout layout;
    };
    std::vector<BufferAccess>  buffers;
    std::vector<TextureAccess> textures;
    for (const auto& edge : m_GPUBufferEdges) {
        auto* resource = &edge.resource->Resolve();
        auto  found    = std::ranges::find(buffers, resource, &BufferAccess::resource);
        if (found == buffers.end())
            buffers.push_back({.resource = resource, .write = edge.write, .access = edge.access, .stage = edge.stage});
        else {
            if (found->write != edge.write) throw std::invalid_argument("Conflicting accesses to an aliased buffer");
            found->access |= edge.access;
            found->stage |= edge.stage;
        }
    }
    for (const auto& edge : m_TextureEdges) {
        auto* resource = &edge.resource->Resolve();
        auto  found    = std::ranges::find(textures, resource, &TextureAccess::resource);
        if (found == textures.end())
            textures.push_back({.resource = resource, .write = edge.write, .access = edge.access, .stage = edge.stage, .layout = edge.layout});
        else {
            if (found->write != edge.write || found->layout != edge.layout) throw std::invalid_argument("Conflicting accesses to an aliased texture");
            found->access |= edge.access;
            found->stage |= edge.stage;
        }
    }
    for (const auto& access : buffers)
        m_GPUBufferBarriers.emplace_back(access.resource->Transition(access.access, access.stage));
    for (const auto& access : textures)
        m_TextureBarriers.emplace_back(access.resource->Transition(access.access, access.layout, access.stage));

    // https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
    if (device.device_type == gfx::Device::Type::DX12) {
        if (GetCommandType() == gfx::CommandType::Copy) {
            for (auto& buffer_barrier : m_GPUBufferBarriers) {
                if (buffer_barrier.src_access != gfx::BarrierAccess::CopySrc &&
                    buffer_barrier.src_access != gfx::BarrierAccess::CopyDst) {
                    buffer_barrier.src_access = gfx::BarrierAccess::None;
                }
                if (buffer_barrier.src_stage != gfx::PipelineStage::Copy) {
                    buffer_barrier.src_stage = gfx::PipelineStage::None;
                }
            }
            for (auto& texture_barrier : m_TextureBarriers) {
                if (texture_barrier.src_access != gfx::BarrierAccess::CopySrc &&
                    texture_barrier.src_access != gfx::BarrierAccess::CopyDst) {
                    texture_barrier.src_access = gfx::BarrierAccess::None;
                }
                if (texture_barrier.src_stage != gfx::PipelineStage::Copy) {
                    texture_barrier.src_stage = gfx::PipelineStage::None;
                }
                if (texture_barrier.src_layout != gfx::TextureLayout::Common) {
                    texture_barrier.src_layout = gfx::TextureLayout::Unkown;
                }
                texture_barrier.dst_layout = gfx::TextureLayout::Common;
            }
        }
    }

    m_ResourceBarriersPrepared = true;
}

void PassNode::ResourceBarrier() {
    if (!m_ResourceBarriersPrepared) {
        PrepareResourceBarriers();
    }

    m_CommandContext->ResourceBarrier({}, m_GPUBufferBarriers, m_TextureBarriers);
    m_ResourceBarriersPrepared = false;
}

void PassNode::PrepareResourceViews() {
    auto& device = m_RenderGraph->GetDevice();
    for (auto& edge : m_GPUBufferEdges) {
        if (!edge.create_view || edge.view) continue;
        auto desc   = edge.view_desc;
        desc.buffer = edge.resource->GetBuffer();
        edge.view   = device.CreateGPUBufferView(std::move(desc));
    }
    for (auto& edge : m_TextureEdges) {
        if (!edge.create_view || edge.view) continue;
        auto desc    = edge.view_desc;
        desc.texture = edge.resource->GetTexture();
        edge.view    = device.CreateTextureView(std::move(desc));
    }
}

void RenderPassNode::Execute() {
    ZoneScoped;
    ZoneName(m_Name.data(), m_Name.size());

    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();

    auto& render_target_view = Resolve(m_RenderTarget);
    cmd.BeginRendering(
        render_target_view,
        m_DepthStencil ? utils::make_optional_ref(Resolve(m_DepthStencil)) : std::nullopt,
        m_ClearRenderTarget,
        m_ClearDepthStencil);

    m_Executor(*m_RenderGraph, *this);

    cmd.EndRendering();
    cmd.End();
}

void ComputePassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void CopyPassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void PresentPassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

}  // namespace hitagi::rg
