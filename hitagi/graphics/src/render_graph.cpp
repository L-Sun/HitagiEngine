#include "render_graph.hpp"

#include <hitagi/utils/overloaded.hpp>

namespace hitagi::gfx {

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

void RenderGraph::Execute() {
    // Prepare all transiant resource used among the frame graph
    // TODO prepare the remaining resource after pruning.
    for (auto&& res : m_InnerResources) {
        std::visit(
            utils::Overloaded{
                [&](resource::Texture& res) {
                    m_Device.InitTexture(res);
                }},
            res);
    }

    // execute all pass
    for (auto&& node : m_PassNodes) {
        ResourceHelper helper(*this, node);
        node.executor->Execute(helper);
    }
}

FrameHandle RenderGraph::Create(std::string_view name, ResourceType resource) {
    // create new resource node
    FrameResourceId id     = m_Resources.size();
    FrameHandle     handle = m_ResourceNodes.size();

    m_InnerResources.emplace_back(std::move(resource));
    m_ResourceNodes.emplace_back(name, id);

    std::visit(
        utils::Overloaded{
            [&](resource::Resource& res) {
                res.name = name;
                m_Resources.emplace_back(&res);
            },
        },
        m_InnerResources.back());

    return handle;
}

void RenderGraph::Present(FrameHandle render_target, const std::shared_ptr<hitagi::gfx::IGraphicsCommandContext>& context) {
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
            context->Present(helper.Get<resource::Texture>(data.output));
        });
}

void RenderGraph::Retire(std::uint64_t fence_value) {
    for (auto res : m_Resources) {
        res->gpu_resource->fence_value = fence_value;
    }
    for (auto& res : m_InnerResources) {
        std::visit(
            utils::Overloaded{
                [&](resource::Resource& res) {
                    m_Device.RetireResource(std::move(res.gpu_resource));
                },
            },
            res);
    }
    m_Retired = true;
}

}  // namespace hitagi::gfx