module render;
import std;

namespace hitagi::render {

void passes::Present::Build(RenderContext& context, rg::TextureHandle input) {
    if (context.swap_chain == nullptr || !context.graph.IsValid(input)) return;

    rg::PresentPassBuilder pass_builder(context.graph);
    pass_builder.From(input);
    pass_builder.SetSwapChain(context.swap_chain);
    pass_builder.Finish();
}

}  // namespace hitagi::render
