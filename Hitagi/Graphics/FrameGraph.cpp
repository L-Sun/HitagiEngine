#include "FrameGraph.hpp"

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

void FrameGraph::Compile() {
    // pruning the FrameGragh
    // ...
    //    generate valid resource id vector used by the next execute function.
    for (size_t i = 0; i < m_ResourcesDesc.size(); i++)
        m_ValidResource.emplace(i, i);
}

void FrameGraph::Execute() {
    // Prepare all resource used among the frame graph
    for (size_t i = 0; i < m_ValidResource.size(); i++) {
        auto id = m_ValidResource[i];
        m_Resources.emplace_back(std::visit(
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
            m_ResourcesDesc[id]));
    }

    // execute all pass
    for (auto&& node : m_PassNodes) {
        ResourceHelper helper(*this, node);
        node.pass->Execute(helper);
    }
}

void FrameGraph::Retire(uint64_t fenceValue) noexcept {
    m_Driver.RetireResources(std::move(m_Resources), fenceValue);
    m_Retired = true;
}

FrameHandle FrameGraph::Create(Desc desc) {
    // create new resource node
    auto handle = m_ResourceNodes.size();
    auto id     = m_ResourcesDesc.size();
    m_ResourcesDesc.emplace_back(std::move(desc));
    m_ResourceNodes.emplace_back(id);
    return handle;
}

}  // namespace Hitagi::Graphics