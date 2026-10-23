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
    auto                dxil   = compiler.LoadProgram(shader.program, gfx::ShaderTarget::DXIL)->Compile(shader.entry, shader.type, shader.specialization_types);
    auto                spirv  = compiler.LoadProgram(shader.program, gfx::ShaderTarget::SPIRV)->Compile(shader.entry, shader.type, shader.specialization_types);
    if (!std::ranges::equal(dxil.vertex_layout, spirv.vertex_layout, [](const auto& a, const auto& b) {
            return a.semantic == b.semantic && a.format == b.format && a.binding == b.binding &&
                   a.offset == b.offset && a.stride == b.stride && a.per_instance == b.per_instance && a.location == b.location;
        })) throw std::invalid_argument("Pipeline vertex layout differs between DXIL and SPIR-V; supply an explicit layout");
    desc.vertex_input_layout = std::move(spirv.vertex_layout);
    return desc;
}
}  // namespace hitagi
