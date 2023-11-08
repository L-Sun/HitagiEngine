#include <hitagi/render_graph/pass_node.hpp>
#include <hitagi/render_graph/render_graph.hpp>

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

namespace hitagi::rg {

PassNode::~PassNode() {
    auto& bindless_utils = m_RenderGraph->GetDevice().GetBindlessUtils();
    for (const auto& [buffer_handle, buffer_edge] : m_GPUBufferEdges) {
        for (auto bindless_handle : buffer_edge.bindless_handles) {
            bindless_utils.DiscardBindlessHandle(bindless_handle);
        }
    }
    for (const auto& [texture_handle, texture_edge] : m_TextureEdges) {
        if (texture_edge.bindless)
            bindless_utils.DiscardBindlessHandle(texture_edge.bindless);
    }
    for (const auto& [sampler_handle, sampler_edge] : m_SamplerEdges) {
        if (sampler_edge.bindless)
            bindless_utils.DiscardBindlessHandle(sampler_edge.bindless);
    }
}

auto PassNode::Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer& {
    if (!m_GPUBufferEdges.contains(buffer)) {
        std::string error_message = fmt::format("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return m_RenderGraph->Resolve(buffer);
}

auto PassNode::Resolve(TextureHandle texture) const -> gfx::Texture& {
    if (!m_TextureEdges.contains(texture)) {
        std::string error_message = fmt::format("Texture({}) is not used in pass({})", texture.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return m_RenderGraph->Resolve(texture);
}

auto PassNode::Resolve(SamplerHandle sampler) const -> gfx::Sampler& {
    if (!m_SamplerEdges.contains(sampler)) {
        std::string error_message = fmt::format("Sampler({}) is not used in pass({})", sampler.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return m_RenderGraph->Resolve(sampler);
}

auto PassNode::Resolve(RenderPipelineHandle pipeline) const -> gfx::RenderPipeline& {
    if (!m_RenderPipelines.contains(pipeline)) {
        std::string error_message = fmt::format("Pipeline({}) is not used in pass({})", pipeline.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return m_RenderGraph->Resolve(pipeline);
}

auto PassNode::Resolve(ComputePipelineHandle pipeline) const -> gfx::ComputePipeline& {
    if (!m_ComputePipelines.contains(pipeline)) {
        std::string error_message = fmt::format("Pipeline({}) is not used in pass({})", pipeline.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return m_RenderGraph->Resolve(pipeline);
}

auto PassNode::GetBindless(GPUBufferHandle buffer, std::size_t index) const noexcept -> gfx::BindlessHandle {
    if (!m_GPUBufferEdges.contains(buffer)) {
        m_RenderGraph->GetLogger()->error("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        return {};
    }

    const auto& buffer_edge = m_GPUBufferEdges.at(buffer);

    if (index >= buffer_edge.bindless_handles.size()) {
        m_RenderGraph->GetLogger()->error("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        return {};
    }

    return buffer_edge.bindless_handles[index];
}

auto PassNode::GetBindless(TextureHandle texture) const noexcept -> gfx::BindlessHandle {
    if (!m_TextureEdges.contains(texture)) {
        m_RenderGraph->GetLogger()->error("Texture({}) is not used in pass({})", texture.index, m_Name);
        return {};
    }

    return m_TextureEdges.at(texture).bindless;
}

auto PassNode::GetBindless(SamplerHandle handle) const noexcept -> gfx::BindlessHandle {
    if (!m_SamplerEdges.contains(handle)) {
        m_RenderGraph->GetLogger()->error("Sampler({}) is not used in pass({})", handle.index, m_Name);
        return {};
    }

    return m_SamplerEdges.at(handle).bindless;
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

void PassNode::ResourceBarrier() {
    auto& device = m_RenderGraph->GetDevice();

    for (const auto& [buffer_handle, buffer_edge] : m_GPUBufferEdges) {
        auto& buffer = Resolve(buffer_handle);
        m_GPUBufferBarriers.emplace_back(buffer.Transition(buffer_edge.access, buffer_edge.stage));
    }

    for (const auto& [texture_handle, texture_edge] : m_TextureEdges) {
        auto& texture = Resolve(texture_handle);
        m_TextureBarriers.emplace_back(texture.Transition(texture_edge.access, texture_edge.layout, texture_edge.stage));
    }

    // https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
    if (device.device_type == gfx::Device::Type::DX12) {
        if (GetCommandType() == gfx::CommandType::Copy) {
            for (auto& buffer_barrier : m_GPUBufferBarriers) {
                if (buffer_barrier.src_access != gfx::BarrierAccess::CopySrc ||
                    buffer_barrier.src_access != gfx::BarrierAccess::CopySrc) {
                    buffer_barrier.src_access = gfx::BarrierAccess::None;
                }
                if (buffer_barrier.src_stage != gfx::PipelineStage::Copy) {
                    buffer_barrier.src_stage = gfx::PipelineStage::None;
                }
            }
            for (auto& texture_barrier : m_TextureBarriers) {
                if (texture_barrier.src_access != gfx::BarrierAccess::CopySrc ||
                    texture_barrier.src_access != gfx::BarrierAccess::CopySrc) {
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

    m_CommandContext->ResourceBarrier({}, m_GPUBufferBarriers, m_TextureBarriers);
}

void PassNode::CreateBindless() {
    ZoneScopedN("Create Bindless");

    auto& bindless_utils = m_RenderGraph->GetDevice().GetBindlessUtils();

    for (auto& [buffer_handle, buffer_edge] : m_GPUBufferEdges) {
        auto&      buffer = Resolve(buffer_handle);
        const auto usage  = buffer.GetDesc().usages;

        if ((!buffer_edge.write && utils::has_flag(usage, gfx::GPUBufferUsageFlags::Constant)) ||
            (buffer_edge.write && utils::has_flag(usage, gfx::GPUBufferUsageFlags::Storage))) {
            for (std::size_t index = 0; index < buffer_edge.num_elements; index++)
                buffer_edge.bindless_handles.emplace_back(bindless_utils.CreateBindlessHandle(buffer, buffer_edge.element_offset + index, buffer_edge.write));
        }
    }
    for (auto& [texture_handle, texture_edge] : m_TextureEdges) {
        auto&      texture = Resolve(texture_handle);
        const auto usage   = texture.GetDesc().usages;

        if ((!texture_edge.write && utils::has_flag(usage, gfx::TextureUsageFlags::SRV)) ||
            (texture_edge.write && utils::has_flag(usage, gfx::TextureUsageFlags::UAV))) {
            texture_edge.bindless = bindless_utils.CreateBindlessHandle(texture, texture_edge.write);
        }
    }

    for (auto& [sampler_handle, sampler_edge] : m_SamplerEdges) {
        sampler_edge.bindless = bindless_utils.CreateBindlessHandle(Resolve(sampler_handle));
    }
}

void RenderPassNode::Execute() {
    ZoneScoped;
    ZoneName(m_Name.data(), m_Name.size());

    CreateBindless();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    cmd.BeginRendering(
        Resolve(m_RenderTarget),
        m_RenderGraph->IsValid(m_DepthStencil)
            ? utils::make_optional_ref(Resolve(m_DepthStencil))
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