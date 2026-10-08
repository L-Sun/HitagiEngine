export module gfx.render_graph:resource_edge;
import std;
import utils;
import gfx.base;
import :type;

export namespace hitagi::rg {

// Access handles belong to one pass instance, not to a resource or a view node.
template <RenderGraphNode::Type T>
struct ResourceEdgeHandle {
    std::uint64_t owner = 0;
    std::size_t   index = std::numeric_limits<std::size_t>::max();
    explicit      operator bool() const noexcept { return owner != 0; }
    bool          operator==(const ResourceEdgeHandle&) const noexcept = default;
};
using GPUBufferEdgeHandle = ResourceEdgeHandle<RenderGraphNode::Type::GPUBuffer>;
using TextureEdgeHandle   = ResourceEdgeHandle<RenderGraphNode::Type::Texture>;

struct GPUBufferEdge {
    GPUBufferNode*                      resource = nullptr;
    bool                                write    = false;
    gfx::BarrierAccess                  access{};
    gfx::PipelineStage                  stage{};
    gfx::GPUBufferViewDesc              view_desc;
    bool                                create_view = true;
    std::shared_ptr<gfx::GPUBufferView> view;
};

struct TextureEdge {
    TextureNode*                      resource = nullptr;
    bool                              write    = false;
    gfx::BarrierAccess                access{};
    gfx::PipelineStage                stage{};
    gfx::TextureLayout                layout{};
    gfx::TextureViewDesc              view_desc;
    bool                              create_view = true;
    std::shared_ptr<gfx::TextureView> view;
};

}  // namespace hitagi::rg
