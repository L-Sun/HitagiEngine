module;
#include <spdlog/spdlog.h>
#include <fmt/color.h>
#include <tracy/Tracy.hpp>
module gfx.render_graph;
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
    pass_base->m_Handle = m_RenderGraph.AllocateNode(pass_base);

    // create edge for move resource:
    // resource_1 -- move --> resource_2(*)        resource_1 -- new_edge -->  pass_2
    //      |                    |read               |                         | write
    //      |                    v         OR        |                         v
    //      +--- new_edge -->  pass_2                +--------- move --- >  resource_2(*)

    for (auto& buffer_edge : pass_base->m_GPUBufferEdges) {
        auto* buffer_node = buffer_edge.resource;
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

    for (const auto& texture_edge : pass_base->m_TextureEdges) {
        auto* texture_node = texture_edge.resource;
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

    for (auto* sampler_node : pass_base->m_Samplers) {
        pass_base->AddInputNode(sampler_node);
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

void PassBuilder::SetPassCullingAllowed(bool allow) noexcept {
    if (m_Invalid) return;
    pass_base->m_Cullable = allow;
}

auto PassBuilder::AddGPUBufferEdge(GPUBufferHandle handle, GPUBufferEdge edge) noexcept -> GPUBufferEdgeHandle {
    if (m_Invalid || m_Finished) return {};
    if (!m_RenderGraph.IsValid(handle)) {
        Invalidate("Invalid buffer handle");
        return {};
    }
    if (edge.view_desc.buffer) {
        Invalidate("Access descriptors must not contain a physical resource");
        return {};
    }
    auto*      node     = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[handle.index].get());
    const auto usages   = node->GetDesc().usages;
    const auto required = edge.access == gfx::BarrierAccess::Vertex    ? gfx::GPUBufferUsageFlags::Vertex
                          : edge.access == gfx::BarrierAccess::Index   ? gfx::GPUBufferUsageFlags::Index
                          : edge.access == gfx::BarrierAccess::CopySrc ? gfx::GPUBufferUsageFlags::CopySrc
                          : edge.access == gfx::BarrierAccess::CopyDst ? gfx::GPUBufferUsageFlags::CopyDst
                          : edge.write                                 ? gfx::GPUBufferUsageFlags::StorageWrite
                                                                       : gfx::GPUBufferUsageFlags::StorageRead;
    if (!utils::has_flag(usages, required)) {
        Invalidate("buffer usage does not support the declared access");
        return {};
    }
    for (const auto& existing : pass_base->m_GPUBufferEdges) {
        if (existing.resource != node) continue;
        if (existing.write != edge.write) {
            Invalidate("Incompatible accesses to the same buffer in one pass");
            return {};
        }
    }
    if (edge.write && node->GetWriter() != nullptr) {
        Invalidate("buffer already has a writer");
        return {};
    }
    edge.resource = node;
    const GPUBufferEdgeHandle result{.owner = pass_base->m_AccessOwner, .index = pass_base->m_GPUBufferEdges.size()};
    pass_base->m_GPUBufferEdges.push_back(std::move(edge));
    return result;
}

auto PassBuilder::AddTextureEdge(TextureHandle handle, TextureEdge edge) noexcept -> TextureEdgeHandle {
    if (m_Invalid || m_Finished) return {};
    if (!m_RenderGraph.IsValid(handle)) {
        Invalidate("Invalid texture handle");
        return {};
    }
    if (edge.view_desc.texture) {
        Invalidate("Access descriptors must not contain a physical resource");
        return {};
    }
    auto*      node     = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[handle.index].get());
    const auto usages   = node->GetDesc().usages;
    const auto required = edge.access == gfx::BarrierAccess::RenderTarget                                                                 ? gfx::TextureUsageFlags::RenderTarget
                          : (edge.access == gfx::BarrierAccess::DepthStencilRead || edge.access == gfx::BarrierAccess::DepthStencilWrite) ? gfx::TextureUsageFlags::DepthStencil
                          : edge.access == gfx::BarrierAccess::CopySrc                                                                    ? gfx::TextureUsageFlags::CopySrc
                          : edge.access == gfx::BarrierAccess::CopyDst                                                                    ? gfx::TextureUsageFlags::CopyDst
                          : edge.write                                                                                                    ? gfx::TextureUsageFlags::UAV
                                                                                                                                          : gfx::TextureUsageFlags::SRV;
    if (!utils::has_flag(usages, required)) {
        Invalidate("texture usage does not support the declared access");
        return {};
    }
    for (const auto& existing : pass_base->m_TextureEdges) {
        if (existing.resource != node) continue;
        if (existing.write != edge.write || existing.layout != edge.layout) {
            Invalidate("Incompatible accesses to the same texture in one pass");
            return {};
        }
    }
    if (edge.write && node->GetWriter() != nullptr) {
        Invalidate("texture already has a writer");
        return {};
    }
    edge.resource = node;
    const TextureEdgeHandle result{.owner = pass_base->m_AccessOwner, .index = pass_base->m_TextureEdges.size()};
    pass_base->m_TextureEdges.push_back(std::move(edge));
    return result;
}

void PassBuilder::AddSamplerEdge(SamplerHandle sampler_handle) noexcept {
    ZoneScoped;

    if (m_Invalid) return;

    if (!m_RenderGraph.IsValid(sampler_handle)) {
        Invalidate(std::format("Read sampler failed: sampler({}) is invalid", sampler_handle.index));
        return;
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph.m_Nodes[sampler_handle.index].get());

    if (pass_base->m_Samplers.contains(sampler_node)) {
        return;
    }

    pass_base->m_Samplers.emplace(sampler_node);
}

RenderPassBuilder::RenderPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<RenderPassNode>(rg)),
      pass(std::static_pointer_cast<RenderPassNode>(pass_base)) {}

void RenderPassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::RenderPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;
}

void RenderPassBuilder::AllowPassCulling(bool allow) noexcept {
    SetPassCullingAllowed(allow);
}

auto RenderPassBuilder::ReadAsVertices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc) noexcept -> GPUBufferEdgeHandle {
    return AddGPUBufferEdge(
        buffer, {
                    .access    = gfx::BarrierAccess::Vertex,
                    .stage     = gfx::PipelineStage::VertexInput,
                    .view_desc = std::move(desc),
                });
}

auto RenderPassBuilder::ReadAsIndices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc) noexcept -> GPUBufferEdgeHandle {
    return AddGPUBufferEdge(
        buffer, {
                    .access    = gfx::BarrierAccess::Index,
                    .stage     = gfx::PipelineStage::VertexInput,
                    .view_desc = std::move(desc),
                });
}

auto RenderPassBuilder::SetRenderTarget(TextureHandle texture, bool clear, gfx::TextureViewDesc desc) noexcept -> TextureEdgeHandle {
    if (m_Invalid) return {};
    if (pass->m_RenderTarget) {
        Invalidate("Attachment already declared");
        return {};
    }
    desc.type       = gfx::TextureViewType::RenderTarget;
    const auto edge = AddTextureEdge(
        texture, {
                     .write     = true,
                     .access    = gfx::BarrierAccess::RenderTarget,
                     .stage     = gfx::PipelineStage::Render,
                     .layout    = gfx::TextureLayout::RenderTarget,
                     .view_desc = std::move(desc),
                 });
    pass->m_RenderTarget      = edge;
    pass->m_ClearRenderTarget = clear;
    return edge;
}

auto RenderPassBuilder::SetDepthStencil(TextureHandle texture, bool clear, gfx::TextureViewDesc desc) noexcept -> TextureEdgeHandle {
    if (m_Invalid) return {};
    if (pass->m_DepthStencil) {
        Invalidate("Attachment already declared");
        return {};
    }
    desc.type       = gfx::TextureViewType::DepthStencil;
    const auto edge = AddTextureEdge(
        texture, {
                     .write     = true,
                     .access    = gfx::BarrierAccess::DepthStencilWrite,
                     .stage     = gfx::PipelineStage::DepthStencil,
                     .layout    = gfx::TextureLayout::DepthStencilWrite,
                     .view_desc = std::move(desc),
                 });
    pass->m_DepthStencil      = edge;
    pass->m_ClearDepthStencil = clear;
    return edge;
}

auto RenderPassBuilder::ReadDepthStencil(TextureHandle texture, gfx::TextureViewDesc desc) noexcept -> TextureEdgeHandle {
    if (m_Invalid) return {};
    if (pass->m_DepthStencil) {
        Invalidate("Attachment already declared");
        return {};
    }
    desc.type       = gfx::TextureViewType::DepthStencil;
    const auto edge = AddTextureEdge(
        texture, {
                     .write     = false,
                     .access    = gfx::BarrierAccess::DepthStencilRead,
                     .stage     = gfx::PipelineStage::DepthStencil,
                     .layout    = gfx::TextureLayout::DepthStencilRead,
                     .view_desc = std::move(desc),
                 });
    pass->m_DepthStencil      = edge;
    pass->m_ClearDepthStencil = false;
    return edge;
}

auto RenderPassBuilder::Read(GPUBufferHandle resource, gfx::GPUBufferViewDesc desc, gfx::PipelineStage stage) noexcept -> GPUBufferEdgeHandle {
    desc.type = gfx::GPUBufferViewType::StorageRead;
    return AddGPUBufferEdge(
        resource, {
                      .write     = false,
                      .access    = gfx::BarrierAccess::ShaderRead,
                      .stage     = stage,
                      .view_desc = std::move(desc),
                  });
}

auto RenderPassBuilder::Read(TextureHandle resource, gfx::TextureViewDesc desc, gfx::PipelineStage stage) noexcept -> TextureEdgeHandle {
    desc.type = gfx::TextureViewType::ShaderRead;
    return AddTextureEdge(
        resource, {
                      .write     = false,
                      .access    = gfx::BarrierAccess::ShaderRead,
                      .stage     = stage,
                      .layout    = gfx::TextureLayout::ShaderRead,
                      .view_desc = std::move(desc),
                  });
}

auto RenderPassBuilder::Write(GPUBufferHandle resource, gfx::GPUBufferViewDesc desc, gfx::PipelineStage stage) noexcept -> GPUBufferEdgeHandle {
    desc.type = gfx::GPUBufferViewType::StorageWrite;
    return AddGPUBufferEdge(
        resource, {
                      .write     = true,
                      .access    = gfx::BarrierAccess::ShaderWrite,
                      .stage     = stage,
                      .view_desc = std::move(desc),
                  });
}

auto RenderPassBuilder::Write(TextureHandle resource, gfx::TextureViewDesc desc, gfx::PipelineStage stage) noexcept -> TextureEdgeHandle {
    desc.type = gfx::TextureViewType::ShaderWrite;
    return AddTextureEdge(
        resource, {
                      .write     = true,
                      .access    = gfx::BarrierAccess::ShaderWrite,
                      .stage     = stage,
                      .layout    = gfx::TextureLayout::ShaderWrite,
                      .view_desc = std::move(desc),
                  });
}

void RenderPassBuilder::AddSampler(SamplerHandle sampler) noexcept {
    AddSamplerEdge(sampler);
}

void RenderPassBuilder::SetExecutor(RenderPassNode::Executor executor) noexcept {
    if (m_Invalid) return;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return;
    }
    pass->m_Executor = std::move(executor);
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

void ComputePassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::ComputePass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: the name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
        return;
    }

    pass->m_Name = name;
}

void ComputePassBuilder::AllowPassCulling(bool allow) noexcept {
    SetPassCullingAllowed(allow);
}

auto ComputePassBuilder::Read(GPUBufferHandle resource, gfx::GPUBufferViewDesc desc) noexcept -> GPUBufferEdgeHandle {
    desc.type = gfx::GPUBufferViewType::StorageRead;
    return AddGPUBufferEdge(
        resource, {
                      .write     = false,
                      .access    = gfx::BarrierAccess::ShaderRead,
                      .stage     = gfx::PipelineStage::ComputeShader,
                      .view_desc = std::move(desc),
                  });
}

auto ComputePassBuilder::Read(TextureHandle resource, gfx::TextureViewDesc desc) noexcept -> TextureEdgeHandle {
    desc.type = gfx::TextureViewType::ShaderRead;
    return AddTextureEdge(
        resource, {
                      .write     = false,
                      .access    = gfx::BarrierAccess::ShaderRead,
                      .stage     = gfx::PipelineStage::ComputeShader,
                      .layout    = gfx::TextureLayout::ShaderRead,
                      .view_desc = std::move(desc),
                  });
}

auto ComputePassBuilder::Write(GPUBufferHandle resource, gfx::GPUBufferViewDesc desc) noexcept -> GPUBufferEdgeHandle {
    desc.type = gfx::GPUBufferViewType::StorageWrite;
    return AddGPUBufferEdge(
        resource, {
                      .write     = true,
                      .access    = gfx::BarrierAccess::ShaderWrite,
                      .stage     = gfx::PipelineStage::ComputeShader,
                      .view_desc = std::move(desc),
                  });
}

auto ComputePassBuilder::Write(TextureHandle resource, gfx::TextureViewDesc desc) noexcept -> TextureEdgeHandle {
    desc.type = gfx::TextureViewType::ShaderWrite;
    return AddTextureEdge(
        resource, {
                      .write     = true,
                      .access    = gfx::BarrierAccess::ShaderWrite,
                      .stage     = gfx::PipelineStage::ComputeShader,
                      .layout    = gfx::TextureLayout::ShaderWrite,
                      .view_desc = std::move(desc),
                  });
}

void ComputePassBuilder::AddSampler(SamplerHandle sampler) noexcept {
    AddSamplerEdge(sampler);
}

void ComputePassBuilder::SetExecutor(ComputePassNode::Executor executor) noexcept {
    if (m_Invalid) return;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return;
    }
    pass->m_Executor = std::move(executor);
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

void CopyPassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::CopyPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name {} already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;
}

void CopyPassBuilder::AllowPassCulling(bool allow) noexcept {
    SetPassCullingAllowed(allow);
}

void CopyPassBuilder::BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept {
    if (src == dst) {
        Invalidate(std::format("Copy buffer failed: src and dst are the same buffer"));
        return;
    }
    AddGPUBufferEdge(
        src,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::Copy,
            .create_view = false,
        });
    AddGPUBufferEdge(
        dst,
        {
            .write       = true,
            .access      = gfx::BarrierAccess::CopyDst,
            .stage       = gfx::PipelineStage::Copy,
            .create_view = false,
        });
}

void CopyPassBuilder::BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer) noexcept {
    AddGPUBufferEdge(
        src,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::Copy,
            .create_view = false,
        });
    AddTextureEdge(
        dst,
        {
            .write       = true,
            .access      = gfx::BarrierAccess::CopyDst,
            .stage       = gfx::PipelineStage::Copy,
            .layout      = gfx::TextureLayout::CopyDst,
            .view_desc   = {.base_mip_level = layer.mip_level, .mip_levels = 1, .base_array_layer = layer.base_array_layer, .layer_count = layer.layer_count},
            .create_view = false,
        });
}

void CopyPassBuilder::TextureToBuffer(TextureHandle src, GPUBufferHandle dst, gfx::TextureSubresourceLayer layer) noexcept {
    AddTextureEdge(
        src,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::Copy,
            .layout      = gfx::TextureLayout::CopySrc,
            .view_desc   = {.base_mip_level = layer.mip_level, .mip_levels = 1, .base_array_layer = layer.base_array_layer, .layer_count = layer.layer_count},
            .create_view = false,
        });
    AddGPUBufferEdge(
        dst,
        {
            .write       = true,
            .access      = gfx::BarrierAccess::CopyDst,
            .stage       = gfx::PipelineStage::Copy,
            .create_view = false,
        });
}

void CopyPassBuilder::TextureToTexture(TextureHandle src, TextureHandle dst, gfx::TextureSubresourceLayer src_layer, gfx::TextureSubresourceLayer dst_layer) noexcept {
    if (src == dst) {
        Invalidate(std::format("Copy texture failed: src and dst are the same texture"));
        return;
    }

    AddTextureEdge(
        src,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::Copy,
            .layout      = gfx::TextureLayout::CopySrc,
            .view_desc   = {.base_mip_level = src_layer.mip_level, .mip_levels = 1, .base_array_layer = src_layer.base_array_layer, .layer_count = src_layer.layer_count},
            .create_view = false,
        });

    AddTextureEdge(
        dst,
        {
            .write       = true,
            .access      = gfx::BarrierAccess::CopyDst,
            .stage       = gfx::PipelineStage::Copy,
            .layout      = gfx::TextureLayout::CopyDst,
            .view_desc   = {.base_mip_level = dst_layer.mip_level, .mip_levels = 1, .base_array_layer = dst_layer.base_array_layer, .layer_count = dst_layer.layer_count},
            .create_view = false,
        });
}

void CopyPassBuilder::SetExecutor(CopyPassNode::Executor executor) noexcept {
    if (m_Invalid) return;

    if (pass->m_Executor) {
        Invalidate("Set executor failed: executor is already set");
        return;
    }
    pass->m_Executor = std::move(executor);
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
        auto  render_target = swap_chain.AcquireTextureForRendering();
        if (!render_target.has_value()) return;

        cmd.ResourceBarrier(
            {}, {}, {{
                        render_target->get().Transition(gfx::BarrierAccess::CopyDst, gfx::TextureLayout::CopyDst),
                    }});
        cmd.CopyTextureRegion(
            pass.m_From->Resolve(),
            {0, 0, 0},
            render_target->get(),
            {0, 0, 0},
            {
                std::min(render_target->get().GetDesc().width, pass.m_From->Resolve().GetDesc().width),
                std::min(render_target->get().GetDesc().height, pass.m_From->Resolve().GetDesc().height),
                1,
            },
            gfx::TextureSubresourceLayer{.mip_level = pass.m_TextureEdges.front().view_desc.base_mip_level, .base_array_layer = pass.m_TextureEdges.front().view_desc.base_array_layer, .layer_count = pass.m_TextureEdges.front().view_desc.layer_count});

        cmd.ResourceBarrier(
            {}, {}, {{
                        render_target->get().Transition(gfx::BarrierAccess::Present, gfx::TextureLayout::Present),
                    }});
    };
}

void PresentPassBuilder::From(TextureHandle texture, gfx::TextureSubresourceLayer layer) noexcept {
    if (pass->m_From) {
        Invalidate(std::format("Present from texture({}) failed: already presented from texture({})", texture.index, pass->m_From->GetName()));
        return;
    }

    AddTextureEdge(
        texture,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::All,
            .layout      = gfx::TextureLayout::CopySrc,
            .view_desc   = {.base_mip_level = layer.mip_level, .mip_levels = 1, .base_array_layer = layer.base_array_layer, .layer_count = layer.layer_count},
            .create_view = false,
        });
    pass->m_From = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture.index].get());
}

void PresentPassBuilder::SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept {
    if (m_Invalid) return;
    if (pass->swap_chain) {
        Invalidate("Set swap chain failed: swap chain is already set");
        return;
    }
    pass->swap_chain = swap_chain;
}

void PresentPassBuilder::Finish() noexcept {
    if (m_Invalid) return;
    if (m_Finished) {
        Invalidate("Finish present pass failed: pass is already finished");
    }
    if (m_Invalid) return;
    {
        if (!pass->swap_chain) {
            Invalidate(std::format("Set swap chain failed: swap chain is nullptr"));
        }

        if (!pass->m_From) {
            Invalidate(std::format("Set present source failed: present source is nullptr"));
        }
    }
    if (m_Invalid) return;

    if (m_RenderGraph.m_PresentPassNode != nullptr) {
        Invalidate("Finish present pass failed: a present pass already exists in the render graph");
        return;
    }

    PassBuilder::Finish();
    m_RenderGraph.m_PresentPassNode = pass;
}

}  // namespace hitagi::rg
