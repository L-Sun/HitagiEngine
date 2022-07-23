#include "render_graph.hpp"

#include <hitagi/utils/overloaded.hpp>

namespace hitagi::graphics {

FrameHandle PassNode::Read(const FrameHandle input) {
    if (!reads.contains(input))
        reads.emplace(input);

    return input;
}
FrameHandle PassNode::Write(RenderGraph& fg, const FrameHandle output) {
    if (writes.contains(output)) return output;
    ResourceNode& old_node = fg.m_ResourceNodes[output];
    // Create new resource node
    FrameHandle ret      = fg.m_ResourceNodes.size();
    auto&       new_node = fg.m_ResourceNodes.emplace_back(old_node.name, old_node.resource);
    new_node.writer      = this;
    new_node.version     = old_node.version + 1;

    writes.emplace(ret);
    return ret;
}

void RenderGraph::Compile() {
    // TODO pruning the FrameGragh
    // ...
    //    generate valid resource id vector used by the next execute function.
}

void RenderGraph::Execute(DeviceAPI& driver) {
    // Prepare all transiant resource used among the frame graph
    // TODO prepare the remaining resource after pruning.
    for (auto&& [id, desc] : m_InnerResourcesDesc) {
        m_Resources[id] = std::visit(
            utils::Overloaded{
                [&, id = id](const TextureBufferDesc& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateTextureBuffer(fmt::format("RenderGraph-texture-{}", id), desc);
                },
                [&, id = id](const DepthBufferDesc& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateDepthBuffer(fmt::format("RenderGraph-depth-buffer-{}", id), desc);
                },
                [&, id = id](const RenderTargetDesc& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateRenderTarget(fmt::format("RenderGraph-render-target-{}", id), desc);
                },
            },
            desc);
    }

    // execute all pass
    for (auto&& node : m_PassNodes) {
        ResourceHelper helper(*this, node);
        node.executor->Execute(helper);
    }
}

void RenderGraph::Retire(uint64_t fence_value, DeviceAPI& driver) noexcept {
    for (const auto& [id, desc] : m_InnerResourcesDesc) {
        driver.RetireResource(m_Resources.at(id), fence_value);
    }
    m_Retired = true;
}

FrameHandle RenderGraph::Create(std::string_view name, Desc desc) {
    // create new resource node
    FrameResourceId id     = m_Resources.size();
    FrameHandle     handle = m_ResourceNodes.size();
    m_Resources.emplace_back(nullptr);
    m_InnerResourcesDesc.emplace(id, desc);
    m_ResourceNodes.emplace_back(name, id);
    return handle;
}

void RenderGraph::Present(FrameHandle render_target, const std::shared_ptr<hitagi::graphics::IGraphicsCommandContext>& context) {
    struct PassData {
        FrameHandle output;
    };
    auto present_pass = AddPass<PassData>(
        "Present",
        [render_target](RenderGraph::Builder& builder, PassData& data) {
            data.output = builder.Read(render_target);
            builder.SideEffect();
        },
        [=](const ResourceHelper& helper, PassData& data) {
            context->Present(helper.Get<RenderTarget>(data.output));
        });
}

}  // namespace hitagi::graphics