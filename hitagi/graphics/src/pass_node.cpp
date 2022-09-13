#include <hitagi/graphics/pass_node.hpp>
#include <hitagi/graphics/render_graph.hpp>

namespace hitagi::gfx {

ResourceHandle PassNode::Read(const ResourceHandle input) {
    if (!reads.contains(input))
        reads.emplace(input);

    return input;
}
ResourceHandle PassNode::Write(RenderGraph& fg, const ResourceHandle output) {
    if (writes.contains(output)) return output;
    ResourceNode& old_node = fg.m_ResourceNodes[output];
    // Create new resource node
    ResourceHandle ret      = fg.m_ResourceNodes.size();
    auto&          new_node = fg.m_ResourceNodes.emplace_back(old_node.name, old_node.resource);
    new_node.writer         = this;
    new_node.version        = old_node.version + 1;

    writes.emplace(ret);
    return ret;
}

}  // namespace hitagi::gfx