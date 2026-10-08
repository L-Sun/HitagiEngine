module;

export module asset:pipeline;
import gfx;
import :resource;
import :shader;

export namespace hitagi::asset {
class RenderPipeline : public Resource {
public:
    RenderPipeline(gfx::RenderPipelineDesc desc, std::pmr::vector<std::shared_ptr<Shader>> shaders)
        : Resource(Type::RenderPipeline, desc.name), m_Desc(std::move(desc)), m_Shaders(std::move(shaders)) {}

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    inline auto GetDesc() const noexcept -> const gfx::RenderPipelineDesc& { return m_Desc; }
    inline auto GetShaders() const noexcept -> std::span<const std::shared_ptr<Shader>> { return m_Shaders; }
    inline auto GetBuiltPipeline() const noexcept -> std::shared_ptr<gfx::RenderPipeline> { return m_Pipeline; }

private:
    gfx::RenderPipelineDesc                          m_Desc;
    std::shared_ptr<gfx::RenderPipeline>             m_Pipeline;
    std::pmr::vector<std::shared_ptr<asset::Shader>> m_Shaders;
};

class ComputePipeline : public Resource {
public:
    ComputePipeline(gfx::ComputePipelineDesc desc, std::shared_ptr<Shader> shader)
        : Resource(Type::ComputePipeline, desc.name), m_Desc(std::move(desc)), m_Shader(std::move(shader)) {}

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    inline auto GetDesc() const noexcept -> const gfx::ComputePipelineDesc& { return m_Desc; }
    inline auto GetShader() const noexcept -> const std::shared_ptr<Shader>& { return m_Shader; }
    inline auto GetBuiltPipeline() const noexcept -> std::shared_ptr<gfx::ComputePipeline> { return m_Pipeline; }

private:
    gfx::ComputePipelineDesc             m_Desc;
    std::shared_ptr<Shader>              m_Shader;
    std::shared_ptr<gfx::ComputePipeline> m_Pipeline;
};
}  // namespace hitagi::asset

namespace hitagi::asset {
void RenderPipeline::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    auto& device = context.device;

    const auto shaders =
        m_Shaders                                                                       //
        | std::views::transform([&](auto& shader) -> std::shared_ptr<gfx::Shader> { 
            shader->Load(context);
            return shader->GetBuiltShader(); })  //
        | std::ranges::to<std::pmr::vector<std::shared_ptr<gfx::Shader>>>();

    if (m_Desc.vertex_input_layout.empty()) {
        const auto vertex_shader = std::ranges::find_if(m_Shaders, [](const auto& shader) {
            return shader && shader->GetDesc().type == gfx::ShaderType::Vertex;
        });
        if (vertex_shader != m_Shaders.end()) {
            m_Desc.vertex_input_layout = context.shader_compiler.ExtractVertexLayout((*vertex_shader)->GetDesc());
        }
    }

    m_Pipeline = hitagi::gfx::RenderPipeline::Create(device, context.bindings, m_Desc, shaders);
    SetLoadState(ResourceLoadState::Loaded);
}

void RenderPipeline::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    m_Pipeline = nullptr;
    SetLoadState(ResourceLoadState::Unloaded);
}

void ComputePipeline::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    if (m_Shader) m_Shader->Load(context);
    m_Pipeline = hitagi::gfx::ComputePipeline::Create(context.device, context.bindings, m_Desc, m_Shader ? m_Shader->GetBuiltShader() : nullptr);
    SetLoadState(ResourceLoadState::Loaded);
}

void ComputePipeline::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    m_Pipeline = nullptr;
    SetLoadState(ResourceLoadState::Unloaded);
}
}  // namespace hitagi::asset
