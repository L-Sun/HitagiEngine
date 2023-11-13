#include <hitagi/render_graph/common_types.hpp>
#include <hitagi/render_graph/render_graph.hpp>

namespace hitagi::rg {

void RenderGraphNode::AddInputNode(RenderGraphNode* node) noexcept {
    if (node == nullptr || node == this) return;
    m_InputNodes.emplace(node);
    node->m_OutputNodes.emplace(this);
}

}  // namespace hitagi::rg