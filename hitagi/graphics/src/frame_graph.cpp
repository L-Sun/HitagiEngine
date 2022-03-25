#include "frame_graph.hpp"

#include <hitagi/core/utils.hpp>

namespace hitagi::graphics {

FrameHandle PassNode::Read(const FrameHandle input) {
    if (!reads.contains(input))
        reads.emplace(input);

    return input;
}
FrameHandle PassNode::Write(FrameGraph& fg, const FrameHandle output) {
    if (writes.contains(output)) return output;
    ResourceNode& old_node = fg.m_ResourceNodes[output];
    // Create new resource node
    FrameHandle ret = fg.m_ResourceNodes.size();
    fg.m_ResourceNodes.emplace_back(old_node.name, old_node.resource, this, old_node.version + 1);
    writes.emplace(ret);
    return ret;
}

void FrameGraph::Compile() {
    // TODO pruning the FrameGragh
    // ...
    //    generate valid resource id vector used by the next execute function.
}

void FrameGraph::Execute(DriverAPI& driver) {
    // Prepare all transiant resource used among the frame graph
    // TODO prepare the remaining resource after pruning.
    for (auto&& [id, desc] : m_InnerResourcesDesc) {
        m_Resources[id] = std::visit(
            Overloaded{
                [&](const TextureBuffer::Description& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateTextureBuffer("", desc);
                },
                [&](const DepthBuffer::Description& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateDepthBuffer("", desc);
                },
                [&](const RenderTarget::Description& desc) -> std::shared_ptr<Resource> {
                    return driver.CreateRenderTarget("", desc);
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

void FrameGraph::Retire(uint64_t fence_value, DriverAPI& driver) noexcept {
    decltype(m_Resources) wait_retire_resources;
    for (const auto& [id, desc] : m_InnerResourcesDesc) {
        wait_retire_resources.emplace_back(std::move(m_Resources.at(id)));
    }

    driver.RetireResources(std::move(wait_retire_resources), fence_value);
    m_Retired = true;
}

FrameHandle FrameGraph::Create(std::string_view name, Desc desc) {
    // create new resource node
    FrameResourceId id     = m_Resources.size();
    FrameHandle     handle = m_ResourceNodes.size();
    m_Resources.emplace_back(nullptr);
    m_InnerResourcesDesc.emplace(id, std::move(desc));
    m_ResourceNodes.emplace_back(name, id);
    return handle;
}

void FrameGraph::Present(FrameHandle render_target, std::shared_ptr<hitagi::graphics::IGraphicsCommandContext> context) {
    struct PassData {
        FrameHandle output;
    };
    auto present_pass = AddPass<PassData>(
        "Present",
        [render_target](FrameGraph::Builder& builder, PassData& data) {
            data.output = builder.Read(render_target);
            builder.SideEffect();
        },
        [=](const ResourceHelper& helper, PassData& data) {
            auto& rt = helper.Get<RenderTarget>(data.output);
            context->Present(rt);
        });
}

}  // namespace hitagi::graphics