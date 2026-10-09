export module render:present;
import std;
import utils;
import math;
import core;
import gfx;
import asset;

import :types;

export namespace hitagi::render::passes {

class Present {
public:
    void Build(RenderContext& context, rg::TextureHandle input);
};

}  // namespace hitagi::render::passes

namespace hitagi::render {

void passes::Present::Build(RenderContext& context, rg::TextureHandle input) {
    if (context.swap_chain == nullptr || !context.graph.IsValid(input)) return;

    rg::PresentPassBuilder pass_builder(context.graph);
    pass_builder.From(input);
    pass_builder.SetSwapChain(context.swap_chain);
    pass_builder.Finish();
}

}  // namespace hitagi::render
