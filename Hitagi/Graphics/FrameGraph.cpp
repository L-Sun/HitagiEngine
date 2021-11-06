#include "FrameGraph.hpp"

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

namespace Hitagi::Graphics {

FrameHandle PassNode::Read(const FrameHandle input) {
    auto pos = std::find(reads.begin(), reads.end(), input);
    if (pos == reads.end())
        reads.emplace_back(input);

    return input;
}
FrameHandle PassNode::Write(FrameGraph& fg, const FrameHandle output) {
    auto pos = std::find(writes.begin(), writes.end(), output);
    if (pos != writes.end()) return output;
    ResourceNode& oldNode = fg.m_ResourceNodes[output];
    // Create new resource node
    FrameHandle ret = fg.m_ResourceNodes.size();
    fg.m_ResourceNodes.emplace_back(oldNode.name, oldNode.resource, this, oldNode.version + 1);
    return ret;
}

void FrameGraph::Compile() {
    // TODO pruning the FrameGragh
    // ...
    //    generate valid resource id vector used by the next execute function.
    for (size_t i = 0; i < m_Resources.size(); i++)
        m_ValidResource.emplace(i);
}

void FrameGraph::Execute(backend::DriverAPI& driver) {
    // Prepare all transiant resource used among the frame graph
    // TODO prepare the remaining resource after pruning.
    for (auto&& [id, desc] : m_ResourcesDesc) {
        m_Resources.at(id) = std::visit(
            overloaded{
                [&](const TextureBuffer::Description& desc) -> ResourceContainer {
                    return driver.CreateTextureBuffer("", desc);
                },
                [&](const DepthBuffer::Description& desc) -> ResourceContainer {
                    return driver.CreateDepthBuffer("", desc);
                },
                [&](const RenderTarget::Description& desc) -> ResourceContainer {
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

void FrameGraph::Retire(uint64_t fenceValue, backend::DriverAPI& driver) noexcept {
    // just retire all transiant resource created by frame graph
    std::vector<ResourceContainer> retire_resources(m_ResourcesDesc.size());
    auto                           iter = retire_resources.begin();
    for (const auto& [id, _] : m_ResourcesDesc) {
        std::swap(*iter, m_Resources.at(id));
        iter++;
    }

    driver.RetireResources(std::move(retire_resources), fenceValue);
    m_Retired = true;
}

FrameHandle FrameGraph::Create(std::string_view name, Desc desc) {
    // create new resource node
    auto handle = m_ResourceNodes.size();
    auto id     = m_Resources.size();
    m_Resources.emplace_back(ResourceContainer{});
    m_ResourcesDesc.emplace(id, std::move(desc));
    m_ResourceNodes.emplace_back(name, id);
    return handle;
}

}  // namespace Hitagi::Graphics