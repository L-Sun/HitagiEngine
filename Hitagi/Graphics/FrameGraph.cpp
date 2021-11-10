#include "FrameGraph.hpp"

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

namespace Hitagi::Graphics {

FrameHandle PassNode::Read(const FrameHandle input) {
    if (!reads.contains(input))
        reads.emplace(input);

    return input;
}
FrameHandle PassNode::Write(FrameGraph& fg, const FrameHandle output) {
    if (writes.contains(output)) return output;
    ResourceNode& oldNode = fg.m_ResourceNodes[output];
    // Create new resource node
    FrameHandle ret = fg.m_ResourceNodes.size();
    fg.m_ResourceNodes.emplace_back(oldNode.name, oldNode.resource, this, oldNode.version + 1);
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
        auto& res = m_InnerResources.emplace_back(std::visit(
            overloaded{
                [&](const TextureBuffer::Description& desc) -> Resource {
                    return driver.CreateTextureBuffer("", desc);
                },
                [&](const DepthBuffer::Description& desc) -> Resource {
                    return driver.CreateDepthBuffer("", desc);
                },
                [&](const RenderTarget::Description& desc) -> Resource {
                    return driver.CreateRenderTarget("", desc);
                },
            },
            desc));
        m_ValidResources.emplace(id, &res);
    }

    // execute all pass
    for (auto&& node : m_PassNodes) {
        ResourceHelper helper(*this, node);
        node.executor->Execute(helper);
    }
}

void FrameGraph::Retire(uint64_t fenceValue, DriverAPI& driver) noexcept {
    driver.RetireResources(std::move(m_InnerResources), fenceValue);
    m_Retired = true;
}

FrameHandle FrameGraph::Create(std::string_view name, Desc desc) {
    // create new resource node
    FrameResourceId id     = m_ResourceCounter++;
    FrameHandle     handle = m_ResourceNodes.size();
    m_InnerResourcesDesc.emplace(id, std::move(desc));
    m_ResourceNodes.emplace_back(name, id);
    return handle;
}

void FrameGraph::Present(FrameHandle renderTarget, std::shared_ptr<Hitagi::Graphics::IGraphicsCommandContext> context) {
    struct PassData {
        FrameHandle output;
    };
    auto presentPass = AddPass<PassData>(
        "Present",
        [&](FrameGraph::Builder& builder, PassData& data) {
            data.output = builder.Read(renderTarget);
            builder.SideEffect();
        },
        [=](const ResourceHelper& helper, PassData& data) {
            auto& rt = helper.Get<RenderTarget>(data.output);
            context->Present(rt);
        });
}

}  // namespace Hitagi::Graphics