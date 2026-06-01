module;


module render;
import std;

namespace hitagi::render {

void passes::Present::Build(RenderContext& context, rg::TextureHandle input) {
    if (context.swap_chain == nullptr || !context.graph.IsValid(input)) return;

    rg::PresentPassBuilder(context.graph)
        .From(input)
        .SetSwapChain(context.swap_chain)
        .Finish();
}

}  // namespace hitagi::render
