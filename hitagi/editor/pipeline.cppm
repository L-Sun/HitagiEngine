export module editor:pipeline;
import engine;

export namespace hitagi {
// Reflection resolves the engine's separate-attribute-buffer convention. Authored
// layouts (including interleaved/instance buffers) remain authoritative.
auto CompilePipelineLayout(gfx::RenderPipelineDesc                         desc,
                           std::span<const std::shared_ptr<asset::Shader>> shaders) -> gfx::RenderPipelineDesc {
    if (!desc.vertex_input_layout.empty()) return desc;
    const auto vertex = std::ranges::find_if(shaders, [](const auto& shader) {
        return shader && shader->GetDesc().type == gfx::ShaderType::Vertex;
    });
    if (vertex == shaders.end()) return desc;
    gfx::ShaderCompiler compiler("EditorPipeline");
    const auto&         shader = (*vertex)->GetDesc();
    desc.vertex_input_layout = compiler.ReflectVertexLayout(shader);
    return desc;
}
}  // namespace hitagi
