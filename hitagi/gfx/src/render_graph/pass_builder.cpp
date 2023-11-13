#include <hitagi/render_graph/render_graph.hpp>
#include <hitagi/render_graph/pass_builder.hpp>
#include <hitagi/gfx/sync.hpp>

#include <spdlog/spdlog.h>
#include <fmt/color.h>
#include <range/v3/view/filter.hpp>
#include <tracy/Tracy.hpp>

namespace hitagi::rg {

PassBuilder::PassBuilder(RenderGraph& render_graph, std::shared_ptr<PassNode> pass_base)
    : m_RenderGraph(render_graph), pass_base(std::move(pass_base)) {
    ZoneScopedN("PassBuilder");
}

PassBuilder::~PassBuilder() {
    if (!m_Finished) {
        m_RenderGraph.GetLogger()->warn("PassBuilder is not finished, remove the node from graph");
    }
}

auto PassBuilder::Finish() -> std::size_t {
    pass_base->m_Handle = m_RenderGraph.m_Nodes.size();
    m_RenderGraph.m_Nodes.emplace_back(pass_base);

    // create edge for move resource:
    // resource_1 -- move --> resource_2(*)        resource_1 -- new_edge -->  pass_2
    //      |                    |read               |                         | write
    //      |                    v         OR        |                         v
    //      +--- new_edge -->  pass_2                +--------- move --- >  resource_2(*)

    for (auto& [buffer_node, buffer_edge] : pass_base->m_GPUBufferEdges) {
        if (buffer_edge.write) {
            buffer_node->AddInputNode(pass_base.get());
        } else {
            pass_base->AddInputNode(buffer_node);
        }

        if (const auto buffer_move_from_node = buffer_node->GetMoveFromNode();
            buffer_move_from_node != nullptr) {
            pass_base->AddInputNode(buffer_move_from_node);
        }
    }

    for (const auto& [texture_node, texture_edge] : pass_base->m_TextureEdges) {
        if (texture_edge.write) {
            texture_node->AddInputNode(pass_base.get());
        } else {
            pass_base->AddInputNode(texture_node);
        }

        if (const auto texture_move_from_node = texture_node->GetMoveFromNode();
            texture_move_from_node != nullptr) {
            pass_base->AddInputNode(texture_move_from_node);
        }
    }

    for (const auto& [sampler_node, sampler_edge] : pass_base->m_SamplerEdges) {
        pass_base->AddInputNode(sampler_node);
    }

    for (const auto render_pipeline_node : pass_base->m_RenderPipelines) {
        pass_base->AddInputNode(render_pipeline_node);
    }

    for (const auto compute_pipeline_node : pass_base->m_ComputePipelines) {
        pass_base->AddInputNode(compute_pipeline_node);
    }

    m_Finished = true;

    return pass_base->m_Handle;
}

void PassBuilder::Invalidate(std::string_view error_message) noexcept {
    if (!m_Invalid) {
        m_Invalid = true;
        m_RenderGraph.m_Logger->error(error_message);
    }
}

void PassBuilder::AddGPUBufferEdge(GPUBufferHandle buffer_handle, GPUBufferEdge new_edge) noexcept {
    ZoneScoped;

    if (m_Invalid) return;

    const auto write_str = new_edge.write ? "write" : "read";

    if (!m_RenderGraph.IsValid(buffer_handle)) {
        Invalidate(fmt::format("Read buffer failed: buffer({}) is invalid", buffer_handle.index));
        return;
    }

    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[buffer_handle.index].get());
    const auto usages      = buffer_node->GetDesc().usages;

    if (!new_edge.write &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::Constant) &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::Vertex) &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::Index) &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::CopySrc)) {
        Invalidate(fmt::format("{} buffer failed: buffer({}) is not a constant buffer", write_str, buffer_node->GetName()));
        return;
    }
    if (new_edge.write &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::Storage) &&
        !utils::has_flag(usages, gfx::GPUBufferUsageFlags::CopyDst)) {
        Invalidate(fmt::format("{} buffer failed: buffer({}) is not a storage buffer", write_str, buffer_node->GetName()));
        return;
    }

    {
        ZoneScopedN("Find existing edge");
        if (pass_base->m_GPUBufferEdges.contains(buffer_node)) {
            if (pass_base->m_GPUBufferEdges[buffer_node] != new_edge) {
                Invalidate(fmt::format("{} buffer: {} buffer({}) repeat with different usage", write_str, write_str, m_RenderGraph.m_Nodes[buffer_handle.index]->GetName()));
            }
            return;
        }
    }

    {
        ZoneScopedN("check multiple write");
        if (new_edge.write) {
            if (const auto writer = buffer_node->GetWriter(); writer != nullptr) {
                Invalidate(fmt::format("Write buffer failed: buffer({}) is written by pass({})", buffer_node->GetName(), writer->GetName()));
                return;
            }
        }
    }

    pass_base->m_GPUBufferEdges.emplace(buffer_node, new_edge);
}

void PassBuilder::AddTextureEdge(TextureHandle texture_handle, TextureEdge new_edge) noexcept {
    ZoneScoped;

    if (m_Invalid) return;

    const auto write_str = new_edge.write ? "write" : "read";

    if (!m_RenderGraph.IsValid(texture_handle)) {
        Invalidate(fmt::format("{} texture failed: texture({}) is invalid", write_str, texture_handle.index));
        return;
    }

    const auto texture_node = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture_handle.index].get());
    const auto usages       = texture_node->GetDesc().usages;

    if (!new_edge.write &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::SRV) &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::CopySrc)) {
        Invalidate(fmt::format("{} texture failed: texture({}) is not readable", write_str, texture_node->GetName()));
        return;
    }
    if (new_edge.write &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::UAV) &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::RenderTarget) &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::DepthStencil) &&
        !utils::has_flag(usages, gfx::TextureUsageFlags::CopyDst)) {
        Invalidate(fmt::format("{} texture failed: texture({}) is not writable", write_str, texture_node->GetName()));
        return;
    }

    {
        ZoneScopedN("Find existing edge");
        if (pass_base->m_TextureEdges.contains(texture_node)) {
            if (pass_base->m_TextureEdges[texture_node] != new_edge) {
                Invalidate(fmt::format("{} texture failed: {} texture({}) repeat with different usage", write_str, write_str, texture_node->GetName()));
            }
            return;
        }
    }

    {
        ZoneScopedN("check multiple write");
        if (new_edge.write) {
            if (const auto writer = texture_node->GetWriter(); writer != nullptr) {
                Invalidate(fmt::format("Write texture failed: texture({}) is written by pass({})", texture_node->GetName(), writer->GetName()));
                return;
            }
        }
    }

    pass_base->m_TextureEdges.emplace(texture_node, new_edge);
}

void PassBuilder::AddSamplerEdge(SamplerHandle sampler_handle, SamplerEdge new_edge) noexcept {
    ZoneScoped;

    if (m_Invalid) return;

    if (!m_RenderGraph.IsValid(sampler_handle)) {
        Invalidate(fmt::format("Read sampler failed: sampler({}) is invalid", sampler_handle.index));
        return;
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph.m_Nodes[sampler_handle.index].get());

    if (pass_base->m_SamplerEdges.contains(sampler_node)) {
        return;
    }

    pass_base->m_SamplerEdges.emplace(sampler_node, new_edge);
}

RenderPassBuilder::RenderPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<RenderPassNode>(rg)),
      pass(std::static_pointer_cast<RenderPassNode>(pass_base)) {}

auto RenderPassBuilder::SetName(std::string_view name) noexcept -> RenderPassBuilder& {
    if (m_Invalid) return *this;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::RenderPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;

    return *this;
}

auto RenderPassBuilder::Read(GPUBufferHandle buffer, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    if (m_Invalid) return *this;
    if (!m_RenderGraph.IsValid(buffer)) {
        Invalidate(fmt::format("Read buffer failed: buffer({}) is invalid", buffer.index));
        return *this;
    }
    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[buffer.index].get());
    return Read(buffer, 0, buffer_node->GetDesc().element_count, stage);
}

auto RenderPassBuilder::Read(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write          = false,
            .access         = gfx::BarrierAccess::Constant,
            .stage          = stage,
            .element_offset = element_offset,
            .num_elements   = num_elements,
        });
    return *this;
}

auto RenderPassBuilder::Read(TextureHandle texture, gfx::TextureSubresourceLayer layer, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    AddTextureEdge(
        texture,
        {
            .write  = false,
            .access = gfx::BarrierAccess::ShaderRead,
            .stage  = stage,
            .layout = gfx::TextureLayout::ShaderRead,
            .layer  = layer,
        });
    return *this;
}

auto RenderPassBuilder::ReadAsVertices(GPUBufferHandle buffer) noexcept -> RenderPassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write  = false,
            .access = gfx::BarrierAccess::Vertex,
            .stage  = gfx::PipelineStage::VertexInput,
        });
    return *this;
}

auto RenderPassBuilder::ReadAsIndices(GPUBufferHandle buffer) noexcept -> RenderPassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write  = false,
            .access = gfx::BarrierAccess::Index,
            .stage  = gfx::PipelineStage::VertexInput,
        });
    return *this;
}

auto RenderPassBuilder::Write(GPUBufferHandle buffer, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    if (m_Invalid) return *this;
    if (!m_RenderGraph.IsValid(buffer)) {
        Invalidate(fmt::format("Read buffer failed: buffer({}) is invalid", buffer.index));
        return *this;
    }
    auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[buffer.index].get());
    return Write(buffer, 0, buffer_node->GetDesc().element_count, stage);
}

auto RenderPassBuilder::Write(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write          = true,
            .access         = gfx::BarrierAccess::ShaderWrite,
            .stage          = stage,
            .element_offset = element_offset,
            .num_elements   = num_elements,
        });
    return *this;
}

auto RenderPassBuilder::Write(TextureHandle texture, gfx::TextureSubresourceLayer layer, gfx::PipelineStage stage) noexcept -> RenderPassBuilder& {
    AddTextureEdge(
        texture,
        {
            .write  = true,
            .access = gfx::BarrierAccess::ShaderWrite,
            .stage  = stage,
            .layout = gfx::TextureLayout::ShaderWrite,
            .layer  = layer,
        });
    return *this;
}

auto RenderPassBuilder::SetRenderTarget(TextureHandle texture, bool clear, gfx::TextureSubresourceLayer layer) noexcept -> RenderPassBuilder& {
    if (pass->m_RenderTarget) {
        Invalidate(fmt::format("Set render target failed: render target is already set"));
        return *this;
    }
    AddTextureEdge(
        texture,
        {
            .write  = true,
            .access = gfx::BarrierAccess::RenderTarget,
            .stage  = gfx::PipelineStage::Render,
            .layout = gfx::TextureLayout::RenderTarget,
            .layer  = layer,
        });
    pass->m_RenderTarget      = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture.index].get());
    pass->m_ClearRenderTarget = clear;
    return *this;
}

auto RenderPassBuilder::SetDepthStencil(TextureHandle texture, bool clear, gfx::TextureSubresourceLayer layer) noexcept -> RenderPassBuilder& {
    if (pass->m_DepthStencil) {
        Invalidate(fmt::format("Set depth stencil failed: depth stencil is already set"));
        return *this;
    }
    AddTextureEdge(
        texture,
        {
            .write  = true,
            .access = gfx::BarrierAccess::DepthStencilWrite,
            .stage  = gfx::PipelineStage::DepthStencil,
            .layout = gfx::TextureLayout::DepthStencilWrite,
            .layer  = layer,
        });
    pass->m_DepthStencil      = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture.index].get());
    pass->m_ClearDepthStencil = clear;
    return *this;
}

auto RenderPassBuilder::AddSampler(SamplerHandle sampler) noexcept -> RenderPassBuilder& {
    AddSamplerEdge(
        sampler,
        SamplerEdge{});
    return *this;
}

auto RenderPassBuilder::AddPipeline(RenderPipelineHandle pipeline) noexcept -> RenderPassBuilder& {
    if (m_Invalid) return *this;

    if (!m_RenderGraph.IsValid(pipeline)) {
        Invalidate(fmt::format("Add pipeline failed: pipeline({}) is invalid", pipeline.index));
        return *this;
    }

    const auto pipeline_node = static_cast<RenderPipelineNode*>(m_RenderGraph.m_Nodes[pipeline.index].get());

    if (!pass->m_RenderPipelines.contains(pipeline_node)) {
        pass->m_RenderPipelines.emplace(pipeline_node);
    }

    return *this;
}

auto RenderPassBuilder::SetExecutor(RenderPassNode::Executor executor) noexcept -> RenderPassBuilder& {
    if (m_Invalid) return *this;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return *this;
    }
    pass->m_Executor = std::move(executor);

    return *this;
}

auto RenderPassBuilder::Finish() noexcept -> RenderPassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish render pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) {
            Invalidate("Finish render pass failed: executor is not set");
        }
        if (!pass->m_RenderTarget) {
            Invalidate("Finish render pass failed: render target is not set");
        }
        if (pass->m_RenderPipelines.empty()) {
            Invalidate("Finish render pass failed: pipeline is not set");
        }
    }
    if (m_Invalid) return {};

    RenderPassHandle handle = PassBuilder::Finish();
    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::RenderPass].emplace(pass->m_Name, handle.index);
    }
    return handle;
}

ComputePassBuilder::ComputePassBuilder(RenderGraph& render_graph)
    : PassBuilder(render_graph, std::make_shared<ComputePassNode>(render_graph)),
      pass(std::static_pointer_cast<ComputePassNode>(pass_base)) {}

auto ComputePassBuilder::SetName(std::string_view name) noexcept -> ComputePassBuilder& {
    if (m_Invalid) return *this;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::ComputePass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: the name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
        return *this;
    }

    pass->m_Name = name;

    return *this;
}

auto ComputePassBuilder::Read(GPUBufferHandle buffer) noexcept -> ComputePassBuilder& {
    if (m_Invalid) return *this;
    if (!m_RenderGraph.IsValid(buffer)) {
        Invalidate(fmt::format("Read buffer failed: buffer({}) is invalid", buffer.index));
        return *this;
    }
    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[buffer.index].get());
    return Read(buffer, 0, buffer_node->GetDesc().element_count);
}

auto ComputePassBuilder::Read(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements) noexcept -> ComputePassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write          = false,
            .access         = gfx::BarrierAccess::Constant,
            .stage          = gfx::PipelineStage::ComputeShader,
            .element_offset = element_offset,
            .num_elements   = num_elements,
        });
    return *this;
}

auto ComputePassBuilder::Read(TextureHandle texture, gfx::TextureSubresourceLayer layer) noexcept -> ComputePassBuilder& {
    AddTextureEdge(
        texture,
        {
            .write  = false,
            .access = gfx::BarrierAccess::ShaderRead,
            .stage  = gfx::PipelineStage::ComputeShader,
            .layout = gfx::TextureLayout::ShaderRead,
            .layer  = layer,
        });
    return *this;
}

auto ComputePassBuilder::Write(GPUBufferHandle buffer) noexcept -> ComputePassBuilder& {
    if (m_Invalid) return *this;

    if (!m_RenderGraph.IsValid(buffer)) {
        Invalidate(fmt::format("Read buffer failed: buffer({}) is invalid", buffer.index));
        return *this;
    }
    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[buffer.index].get());
    return Write(buffer, 0, buffer_node->GetDesc().element_count);
}

auto ComputePassBuilder::Write(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements) noexcept -> ComputePassBuilder& {
    AddGPUBufferEdge(
        buffer,
        {
            .write          = true,
            .access         = gfx::BarrierAccess::ShaderWrite,
            .stage          = gfx::PipelineStage::ComputeShader,
            .element_offset = element_offset,
            .num_elements   = num_elements,
        });
    return *this;
}

auto ComputePassBuilder::Write(TextureHandle texture, gfx::TextureSubresourceLayer layer) noexcept -> ComputePassBuilder& {
    AddTextureEdge(
        texture,
        {
            .write  = true,
            .access = gfx::BarrierAccess::ShaderWrite,
            .stage  = gfx::PipelineStage::ComputeShader,
            .layout = gfx::TextureLayout::ShaderWrite,
            .layer  = layer,
        });
    return *this;
}

auto ComputePassBuilder::AddSampler(SamplerHandle sampler) noexcept -> ComputePassBuilder& {
    AddSamplerEdge(sampler, SamplerEdge{});
    return *this;
}

auto ComputePassBuilder::AddPipeline(ComputePipelineHandle pipeline) noexcept -> ComputePassBuilder& {
    if (m_Invalid) return *this;

    if (!m_RenderGraph.IsValid(pipeline)) {
        Invalidate(fmt::format("Add pipeline failed: pipeline({}) is invalid", pipeline.index));
        return *this;
    }

    const auto pipeline_node = static_cast<ComputePipelineNode*>(m_RenderGraph.m_Nodes[pipeline.index].get());

    if (!pass->m_ComputePipelines.contains(pipeline_node)) {
        pass->m_ComputePipelines.emplace(pipeline_node);
    }

    return *this;
}

auto ComputePassBuilder::SetExecutor(ComputePassNode::Executor executor) noexcept -> ComputePassBuilder& {
    if (m_Invalid) return *this;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return *this;
    }
    pass->m_Executor = std::move(executor);

    return *this;
}

auto ComputePassBuilder::Finish() noexcept -> ComputePassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish compute pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) {
            Invalidate("Finish compute pass failed: executor is not set");
        }
        if (pass->m_ComputePipelines.empty()) {
            Invalidate("Finish compute pass failed: pipeline is not set");
        }
    }
    if (m_Invalid) return {};

    ComputePassHandle handle = PassBuilder::Finish();
    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::ComputePass].emplace(pass->m_Name, handle.index);
    }
    return handle;
}

CopyPassBuilder::CopyPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<CopyPassNode>(rg)),
      pass(std::static_pointer_cast<CopyPassNode>(pass_base)) {}

auto CopyPassBuilder::SetName(std::string_view name) noexcept -> CopyPassBuilder& {
    if (m_Invalid) return *this;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::CopyPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name {} already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;

    return *this;
}

auto CopyPassBuilder::BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept -> CopyPassBuilder& {
    if (src == dst) {
        Invalidate(fmt::format("Copy buffer failed: src and dst are the same buffer"));
        return *this;
    }
    AddGPUBufferEdge(
        src,
        {
            .write  = false,
            .access = gfx::BarrierAccess::CopySrc,
            .stage  = gfx::PipelineStage::Copy,
        });
    AddGPUBufferEdge(
        dst,
        {
            .write  = true,
            .access = gfx::BarrierAccess::CopyDst,
            .stage  = gfx::PipelineStage::Copy,
        });
    return *this;
}

auto CopyPassBuilder::BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer) noexcept -> CopyPassBuilder& {
    AddGPUBufferEdge(
        src,
        {
            .write  = false,
            .access = gfx::BarrierAccess::CopySrc,
            .stage  = gfx::PipelineStage::Copy,
        });
    AddTextureEdge(
        dst,
        {
            .write  = true,
            .access = gfx::BarrierAccess::CopyDst,
            .stage  = gfx::PipelineStage::Copy,
            .layout = gfx::TextureLayout::CopyDst,
            .layer  = layer,
        });
    return *this;
}

auto CopyPassBuilder::TextureToTexture(TextureHandle src, TextureHandle dst, gfx::TextureSubresourceLayer src_layer, gfx::TextureSubresourceLayer dst_layer) noexcept -> CopyPassBuilder& {
    if (src == dst) {
        Invalidate(fmt::format("Copy texture failed: src and dst are the same texture"));
        return *this;
    }

    AddTextureEdge(
        src,
        {
            .write  = false,
            .access = gfx::BarrierAccess::CopySrc,
            .stage  = gfx::PipelineStage::Copy,
            .layout = gfx::TextureLayout::CopySrc,
            .layer  = src_layer,
        });

    AddTextureEdge(
        dst,
        {
            .write  = true,
            .access = gfx::BarrierAccess::CopyDst,
            .stage  = gfx::PipelineStage::Copy,
            .layout = gfx::TextureLayout::CopyDst,
            .layer  = dst_layer,
        });

    return *this;
}

auto CopyPassBuilder::SetExecutor(CopyPassNode::Executor executor) noexcept -> CopyPassBuilder& {
    if (m_Invalid) return *this;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return *this;
    }
    pass->m_Executor = std::move(executor);

    return *this;
}

auto CopyPassBuilder::Finish() noexcept -> CopyPassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish copy pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) Invalidate("Finish copy pass failed: executor is not set");
    }
    if (m_Invalid) return {};

    CopyPassHandle handle = PassBuilder::Finish();

    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::CopyPass].emplace(pass->m_Name, handle.index);
    }

    return handle;
}

PresentPassBuilder::PresentPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<PresentPassNode>(rg)),
      pass(std::static_pointer_cast<PresentPassNode>(pass_base))

{
    pass->m_Name     = "PresentPass";
    pass->m_Executor = [](const RenderGraph& rg, const PresentPassNode& pass) {
        auto& cmd           = pass.GetCmd();
        auto& swap_chain    = *pass.swap_chain;
        auto& render_target = swap_chain.AcquireTextureForRendering();

        cmd.ResourceBarrier(
            {}, {}, {{
                        render_target.Transition(gfx::BarrierAccess::CopyDst, gfx::TextureLayout::CopyDst),
                    }});
        cmd.CopyTextureRegion(
            pass.m_From->Resolve(),
            {0, 0, 0},
            render_target,
            {0, 0, 0},
            {
                std::min(render_target.GetDesc().width, pass.m_From->Resolve().GetDesc().width),
                std::min(render_target.GetDesc().height, pass.m_From->Resolve().GetDesc().height),
                1,
            },
            pass.m_TextureEdges.begin()->second.layer);

        cmd.ResourceBarrier(
            {}, {}, {{
                        render_target.Transition(gfx::BarrierAccess::Present, gfx::TextureLayout::Present),
                    }});
    };
}

auto PresentPassBuilder::From(TextureHandle texture, gfx::TextureSubresourceLayer layer) noexcept -> PresentPassBuilder& {
    if (pass->m_From) {
        Invalidate(fmt::format("Present from texture({}) failed: already presented from texture({})", texture.index, pass->m_From->GetName()));
        return *this;
    }

    AddTextureEdge(
        texture,
        {
            .write  = false,
            .access = gfx::BarrierAccess::CopySrc,
            .stage  = gfx::PipelineStage::All,
            .layout = gfx::TextureLayout::CopySrc,
            .layer  = layer,
        });
    pass->m_From = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture.index].get());
    return *this;
}

auto PresentPassBuilder::SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept -> PresentPassBuilder& {
    if (m_Invalid) return *this;
    if (pass->swap_chain) {
        Invalidate("Set swap chain failed: swap chain is already set");
        return *this;
    }
    pass->swap_chain = swap_chain;
    return *this;
}

void PresentPassBuilder::Finish() noexcept {
    if (m_Invalid) return;
    if (m_Finished) {
        Invalidate("Finish present pass failed: pass is already finished");
    }
    if (m_Invalid) return;
    {
        if (!pass->swap_chain) {
            Invalidate(fmt::format("Set swap chain failed: swap chain is nullptr"));
        }

        if (!pass->m_From) {
            Invalidate(fmt::format("Set present source failed: present source is nullptr"));
        }
    }
    if (m_Invalid) return;

    PassBuilder::Finish();
    m_RenderGraph.m_PresentPassNode = pass;

    m_Finished = true;
}

}  // namespace hitagi::rg