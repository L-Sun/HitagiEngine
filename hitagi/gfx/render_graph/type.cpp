module;
module gfx.render_graph;
namespace hitagi::rg {

void RenderGraphNode::AddInputNode(RenderGraphNode* node) noexcept {
    if (node == nullptr || node == this) return;
    m_InputNodes.emplace(node);
    node->m_OutputNodes.emplace(this);
}

}  // namespace hitagi::rg