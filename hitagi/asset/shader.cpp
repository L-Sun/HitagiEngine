module;

export module asset:shader;
import gfx;
import :resource;

export namespace hitagi::asset {
class Shader : public Resource {
public:
    Shader(gfx::ShaderDesc desc) : Resource(Type::Shader, desc.name), m_Desc(std::move(desc)) {}

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

    inline auto GetDesc() const noexcept -> const gfx::ShaderDesc& { return m_Desc; }
    inline auto GetBuiltShader() const noexcept -> std::shared_ptr<gfx::Shader> { return m_Shader; }

private:
    gfx::ShaderDesc              m_Desc;
    std::shared_ptr<gfx::Shader> m_Shader;
};

}  // namespace hitagi::asset

namespace hitagi::asset {
void Shader::Load(const ResourceLoadContext& context) {
    if (GetLoadState() == ResourceLoadState::Loaded) return;
    m_Shader = hitagi::gfx::Shader::Create(context.device, context.shader_compiler, m_Desc);
    SetLoadState(ResourceLoadState::Loaded);
}

void Shader::Unload() {
    if (GetLoadState() != ResourceLoadState::Loaded) return;
    m_Shader = nullptr;
    SetLoadState(ResourceLoadState::Unloaded);
}

}  // namespace hitagi::asset
