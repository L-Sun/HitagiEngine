module;
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>
module gfx.render_graph;
namespace hitagi::rg {

PassNode::~PassNode() {
    auto& bindless_utils = m_RenderGraph->GetDevice().GetBindlessUtils();
    for (const auto& [sampler_handle, sampler_edge] : m_SamplerEdges) {
        if (sampler_edge.bindless)
            bindless_utils.DiscardBindlessHandle(sampler_edge.bindless);
    }
}

auto PassNode::Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer& {
    if (!m_RenderGraph->IsValid(buffer)) {
        std::string error_message = std::format("Buffer({}) is not valid handle", buffer.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph->m_Nodes[buffer.index].get());

    if (!m_GPUBufferEdges.contains(buffer_node)) {
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

    if (!m_TextureEdges.contains(texture_node)) {
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

    if (!m_SamplerEdges.contains(sampler_node)) {
        std::string error_message = std::format("Sampler({}) is not used in pass({})", sampler.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return sampler_node->Resolve();
}

auto PassNode::GetBindless(GPUBufferHandle buffer, std::size_t index) const noexcept -> gfx::BindlessHandle {
    if (!m_RenderGraph->IsValid(buffer)) {
        std::string error_message = std::format("Buffer({}) is not valid handle", buffer.index);
        m_RenderGraph->GetLogger()->error(error_message);
        return {};
    }

    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph->m_Nodes[buffer.index].get());

    if (!m_GPUBufferEdges.contains(buffer_node)) {
        m_RenderGraph->GetLogger()->error("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        return {};
    }

    const auto& buffer_edge = m_GPUBufferEdges.at(buffer_node);

    if (index >= buffer_edge.bindless_views.size() || !buffer_edge.bindless_views[index]) {
        m_RenderGraph->GetLogger()->error("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        return {};
    }

    return buffer_edge.bindless_views[index]->GetBindlessHandle();
}

auto PassNode::GetBindless(TextureHandle texture) const noexcept -> gfx::BindlessHandle {
    if (!m_RenderGraph->IsValid(texture)) {
        std::string error_message = std::format("Texture({}) is not valid handle", texture.index);
        m_RenderGraph->GetLogger()->error(error_message);
        return {};
    }

    const auto texture_node = static_cast<TextureNode*>(m_RenderGraph->m_Nodes[texture.index].get());

    if (!m_TextureEdges.contains(texture_node)) {
        m_RenderGraph->GetLogger()->error("Texture({}) is not used in pass({})", texture.index, m_Name);
        return {};
    }

    const auto& texture_edge = m_TextureEdges.at(texture_node);
    if (!texture_edge.bindless_view) {
        m_RenderGraph->GetLogger()->error("Texture({}) is not used in pass({})", texture.index, m_Name);
        return {};
    }

    return texture_edge.bindless_view->GetBindlessHandle();
}

auto PassNode::GetBindless(SamplerHandle handle) const noexcept -> gfx::BindlessHandle {
    if (!m_RenderGraph->IsValid(handle)) {
        std::string error_message = std::format("Sampler({}) is not valid handle", handle.index);
        m_RenderGraph->GetLogger()->error(error_message);
        return {};
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph->m_Nodes[handle.index].get());

    if (!m_SamplerEdges.contains(sampler_node)) {
        m_RenderGraph->GetLogger()->error("Sampler({}) is not used in pass({})", handle.index, m_Name);
        return {};
    }

    return m_SamplerEdges.at(sampler_node).bindless;
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

    // ! We defer bindless handle creation at the front of pass execution to avoid READ_AFTER_WRITE hazard
    // for example: pass_1 -> resource_1 -> pass_2
    // execution order:
    //      not deferred: pass_1::create bindless   +---> pass_2::create bindless  +---> pass_1::execute  +--> pass_2::execute
    //                      |                       |     |                        |     |                |    |
    //                      v                       |     v                        |     v                |    v
    //                     [resource_1 write bindless]    resource_1 read bindless +     write resource_1 +    read resource_1
    //
    // Although we use barrier to avoid this hazard, but the resource_1 read bindless has been created before pass execution
    // and the global bindless descriptor heap(set) is set at the beginning of pass execution.
    // So in the pass_1::execute, there is a resource_1 read bindless handle, but resource_1 is no longer memory available required by resource_1 read bindless.
    //
    //
    //        deferred: pass_1::create bindless   +---> pass_1::execute    +---> pass_2::create bindless    +--> pass_2::execute
    //                    |                       |     |                  |     |                          |    |
    //                    v                       |     v                  |     |                          |    v
    //                   [resource_1 write bindless]    write resource_1 --+     resource_1 read bindless --+    read resource
    //
    // In this case, the resource_1 read bindless handle is created after pass_1::execute, so there is no hazard.
}

void PassNode::PrepareResourceBarriers() {
    auto& device = m_RenderGraph->GetDevice();

    m_GPUBufferBarriers.clear();
    m_TextureBarriers.clear();

    for (const auto& [buffer_node, buffer_edge] : m_GPUBufferEdges) {
        auto& buffer = buffer_node->Resolve();
        m_GPUBufferBarriers.emplace_back(buffer.Transition(buffer_edge.access, buffer_edge.stage));
    }

    for (const auto& [texture_node, texture_edge] : m_TextureEdges) {
        auto& texture = texture_node->Resolve();
        m_TextureBarriers.emplace_back(texture.Transition(texture_edge.access, texture_edge.layout, texture_edge.stage));
    }

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

void PassNode::CreateBindless() {
    ZoneScopedN("Create Bindless");

    auto& device         = m_RenderGraph->GetDevice();
    auto& bindless_utils = device.GetBindlessUtils();

    for (auto& [buffer_node, buffer_edge] : m_GPUBufferEdges) {
        const auto buffer = buffer_node->GetBuffer();
        if (!buffer) continue;

        const auto usage = buffer->GetDesc().usages;
        if (buffer_edge.bindless_views.empty() &&
            ((!buffer_edge.write && utils::has_flag(usage, gfx::GPUBufferUsageFlags::Constant)) ||
             (!buffer_edge.write && utils::has_flag(usage, gfx::GPUBufferUsageFlags::Storage)) ||
             (buffer_edge.write && utils::has_flag(usage, gfx::GPUBufferUsageFlags::Storage)))) {
            const auto view_type = buffer_edge.write
                                       ? gfx::GPUBufferViewType::StorageWrite
                                   : utils::has_flag(usage, gfx::GPUBufferUsageFlags::Constant)
                                       ? gfx::GPUBufferViewType::Constant
                                       : gfx::GPUBufferViewType::StorageRead;

            const auto element_stride = view_type == gfx::GPUBufferViewType::Constant
                                            ? gfx::ConstantBufferElementSize(buffer_edge.element_size)
                                            : buffer_edge.element_size;
            for (std::size_t index = 0; index < buffer_edge.num_elements; index++) {
                auto view = device.CreateGPUBufferView(gfx::GPUBufferViewDesc{
                    .name          = std::pmr::string(std::format("{}-view-{}", buffer->GetName(), index)),
                    .buffer        = buffer,
                    .type          = view_type,
                    .offset        = (buffer_edge.element_offset + index) * element_stride,
                    .element_size  = buffer_edge.element_size,
                    .element_count = 1,
                });
                buffer_edge.bindless_views.emplace_back(std::move(view));
            }
        }
    }
    for (auto& [texture_node, texture_edge] : m_TextureEdges) {
        const auto texture = texture_node->GetTexture();
        if (!texture) continue;

        const auto usage = texture->GetDesc().usages;

        if ((!texture_edge.write && utils::has_flag(usage, gfx::TextureUsageFlags::SRV)) ||
            (texture_edge.write && utils::has_flag(usage, gfx::TextureUsageFlags::UAV))) {
            texture_edge.bindless_view = device.CreateTextureView(gfx::TextureViewDesc{
                .name             = std::pmr::string(std::format("{}-view", texture->GetName())),
                .texture          = texture,
                .type             = texture_edge.write ? gfx::TextureViewType::ShaderWrite : gfx::TextureViewType::ShaderRead,
                .base_mip_level   = texture_edge.layer.mip_level,
                .mip_levels       = texture_edge.write ? 1u : 0u,
                .base_array_layer = texture_edge.layer.base_array_layer,
                .layer_count      = texture_edge.layer.layer_count,
            });
        }
    }

    for (auto& [sampler_node, sampler_edge] : m_SamplerEdges) {
        sampler_edge.bindless = bindless_utils.CreateBindlessHandle(sampler_node->Resolve());
    }
}

void RenderPassNode::Execute() {
    ZoneScoped;
    ZoneName(m_Name.data(), m_Name.size());

    CreateBindless();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();

    auto& device              = m_RenderGraph->GetDevice();
    auto  render_target       = m_RenderTarget->GetTexture();
    auto& render_target_edge  = m_TextureEdges.at(m_RenderTarget);
    auto  render_target_view  = device.CreateTextureView(gfx::TextureViewDesc{
         .name             = std::pmr::string(std::format("{}-rtv", render_target->GetName())),
         .texture          = render_target,
         .type             = gfx::TextureViewType::RenderTarget,
         .base_mip_level   = render_target_edge.layer.mip_level,
         .mip_levels       = 1,
         .base_array_layer = render_target_edge.layer.base_array_layer,
         .layer_count      = render_target_edge.layer.layer_count,
    });

    std::shared_ptr<gfx::TextureView> depth_stencil_view;
    if (m_DepthStencil) {
        auto  depth_stencil      = m_DepthStencil->GetTexture();
        auto& depth_stencil_edge = m_TextureEdges.at(m_DepthStencil);
        depth_stencil_view       = device.CreateTextureView(gfx::TextureViewDesc{
                  .name             = std::pmr::string(std::format("{}-dsv", depth_stencil->GetName())),
                  .texture          = depth_stencil,
                  .type             = gfx::TextureViewType::DepthStencil,
                  .base_mip_level   = depth_stencil_edge.layer.mip_level,
                  .mip_levels       = 1,
                  .base_array_layer = depth_stencil_edge.layer.base_array_layer,
                  .layer_count      = depth_stencil_edge.layer.layer_count,
        });
    }

    cmd.BeginRendering(
        *render_target_view,
        depth_stencil_view
            ? utils::make_optional_ref(*depth_stencil_view)
            : std::nullopt,
        m_ClearRenderTarget,
        m_ClearDepthStencil);

    m_Executor(*m_RenderGraph, *this);

    cmd.EndRendering();
    cmd.End();
}

void ComputePassNode::Execute() {
    CreateBindless();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void CopyPassNode::Execute() {
    CreateBindless();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void PresentPassNode::Execute() {
    CreateBindless();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

}  // namespace hitagi::rg
