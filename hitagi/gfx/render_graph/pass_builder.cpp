module;
#include "interop/tracy_macros.hpp"

export module gfx.render_graph:pass_builder;
import interop.tracy;
import std;
import utils;
import gfx.base;
import :type;
import :resource_edge;
import :resource_node;
import :pass_node;

export namespace hitagi::rg {

class PassBuilder {
public:
    friend RenderGraph;

    PassBuilder(const PassBuilder&)            = delete;
    PassBuilder(PassBuilder&&)                 = default;
    PassBuilder& operator=(const PassBuilder&) = delete;
    PassBuilder& operator=(PassBuilder&&)      = delete;

protected:
    PassBuilder(RenderGraph& render_graph, std::shared_ptr<PassNode> pass_base);

    ~PassBuilder();

    void Invalidate(std::string_view error_message) noexcept;
    void SetPassCullingAllowed(bool allow) noexcept;
    auto AddGPUBufferEdge(GPUBufferHandle buffer_handle, GPUBufferEdge edge) noexcept -> GPUBufferEdgeHandle;
    auto AddTextureEdge(TextureHandle texture_handle, TextureEdge edge) noexcept -> TextureEdgeHandle;
    void AddSamplerEdge(SamplerHandle sampler_handle) noexcept;

    auto Finish() -> std::size_t;

    RenderGraph& m_RenderGraph;
    bool         m_Invalid  = false;
    bool         m_Finished = false;

    std::shared_ptr<PassNode> pass_base;
};

class RenderPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    RenderPassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    auto Read(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> GPUBufferEdgeHandle;
    auto Read(TextureHandle texture, gfx::TextureViewDesc desc = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> TextureEdgeHandle;
    auto ReadAsVertices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto ReadAsIndices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;

    auto Write(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> GPUBufferEdgeHandle;
    auto Write(TextureHandle texture, gfx::TextureViewDesc desc = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> TextureEdgeHandle;

    auto SetRenderTarget(TextureHandle texture, bool clear = false, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;
    auto SetDepthStencil(TextureHandle texture, bool clear = false, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;
    auto ReadDepthStencil(TextureHandle texture, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;

    void AddSampler(SamplerHandle sampler) noexcept;

    void SetExecutor(RenderPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> RenderPassHandle;

private:
    std::shared_ptr<RenderPassNode> pass;
};

class ComputePassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    ComputePassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    auto Read(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto Read(TextureHandle texture, gfx::TextureViewDesc desc = {}) noexcept -> TextureEdgeHandle;

    auto Write(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto Write(TextureHandle texture, gfx::TextureViewDesc desc = {}) noexcept -> TextureEdgeHandle;

    void AddSampler(SamplerHandle sampler) noexcept;

    void SetExecutor(ComputePassNode::Executor executor) noexcept;

    auto Finish() noexcept -> ComputePassHandle;

private:
    std::shared_ptr<ComputePassNode> pass;
};

class CopyPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    CopyPassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    void BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept;
    void BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void TextureToBuffer(TextureHandle src, GPUBufferHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void TextureToTexture(TextureHandle src, TextureHandle dst, gfx::TextureSubresourceLayer src_layer = {}, gfx::TextureSubresourceLayer dst_layer = {}) noexcept;

    void SetExecutor(CopyPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> CopyPassHandle;

private:
    std::shared_ptr<CopyPassNode> pass;
};

class PresentPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    PresentPassBuilder(RenderGraph& render_graph);

    void From(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept;

    void Finish() noexcept;

private:
    std::shared_ptr<PresentPassNode> pass;
};

}  // namespace hitagi::rg

namespace hitagi::rg {

PassBuilder::PassBuilder(RenderGraph& render_graph, std::shared_ptr<PassNode> pass_base)
    : m_RenderGraph(render_graph), pass_base(std::move(pass_base)) {
    ZoneScopedN("PassBuilder");
}

void PassBuilder::SetPassCullingAllowed(bool allow) noexcept {
    if (m_Invalid) return;
    pass_base->m_Cullable = allow;
}

RenderPassBuilder::RenderPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<RenderPassNode>(rg)),
      pass(std::static_pointer_cast<RenderPassNode>(pass_base)) {}

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

ComputePassBuilder::ComputePassBuilder(RenderGraph& render_graph)
    : PassBuilder(render_graph, std::make_shared<ComputePassNode>(render_graph)),
      pass(std::static_pointer_cast<ComputePassNode>(pass_base)) {}

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

CopyPassBuilder::CopyPassBuilder(RenderGraph& rg)
    : PassBuilder(rg, std::make_shared<CopyPassNode>(rg)),
      pass(std::static_pointer_cast<CopyPassNode>(pass_base)) {}

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

void PresentPassBuilder::SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept {
    if (m_Invalid) return;
    if (pass->swap_chain) {
        Invalidate("Set swap chain failed: swap chain is already set");
        return;
    }
    pass->swap_chain = swap_chain;
}

}  // namespace hitagi::rg
