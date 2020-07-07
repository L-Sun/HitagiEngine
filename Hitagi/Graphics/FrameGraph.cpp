#include "FrameGraph.hpp"
#include "DriverAPI.hpp"

template <typename>
inline constexpr bool always_false_v = false;

namespace Hitagi::Graphics {

FrameHandle PassNode::Read(FrameHandle input) {
    auto pos = std::find(reads.begin(), reads.end(), input);
    if (pos == reads.end())
        reads.emplace_back(input);

    return input;
}
FrameHandle PassNode::Write(FrameGraph& fg, FrameHandle output) {
    auto pos = std::find(writes.begin(), writes.end(), output);
    if (pos != writes.end()) return output;
    ResourceNode& oldNode = fg.m_ResourceNodes[output];
    // Create new resource node
    FrameHandle ret = fg.m_ResourceNodes.size();
    fg.m_ResourceNodes.emplace_back(oldNode.resource, this, oldNode.version + 1);
    return ret;
}
FrameGraph::FrameGraph(backend::DriverAPI& driver) : m_Driver(driver) {}

void FrameGraph::Compile() {
    // pruning the FrameGragh
    // ...
    //    generate valid resource id vector used by the next execute function.
    for (size_t i = 0; i < m_Resources.size(); i++)
        m_ValidResource.push_back(i);
}

void FrameGraph::Execute() {
    // Prepare all resource used among the frame graph
    for (auto id : m_ValidResource) {
        m_Resources[id].second = std::visit(
            [this](auto& arg) -> ResourceContainer {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, TextureBuffer::Description>)
                    return m_Driver.CreateTextureBuffer("", arg);
                else if constexpr (std::is_same_v<T, DepthBuffer::Description>)
                    return m_Driver.CreateDepthBuffer("", arg);
                else if constexpr (std::is_same_v<T, RenderTarget::Description>)
                    return m_Driver.CreateRenderTarget("", arg);
                else
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
            },
            m_Resources[id].first);
    }

    // execute all pass
    for (auto&& node : m_PassNodes) {
        ResourceHelper helper(*this, node);
        node.pass->Execute(helper);
    }
}

FrameHandle FrameGraph::Create(Desc desc) {
    // create new resource node
    auto handle = m_ResourceNodes.size();
    auto id     = m_Resources.size();
    m_Resources.emplace_back(std::move(desc), ResourceContainer{});
    m_ResourceNodes.emplace_back(id);
    return handle;
}

}  // namespace Hitagi::Graphics