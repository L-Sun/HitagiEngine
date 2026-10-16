export module anime_viewer;
import engine;
import std;

export namespace game::anime_viewer {
using namespace hitagi;

struct FrameImages {
    std::map<std::string, rg::TextureHandle> stages;
    rg::TextureHandle                        final;
    std::size_t                              draws = 0;
};

struct RenderSettings {
    std::uint32_t scale   = 2;
    bool          shadows = true, face_lighting = true, specular = true;
    bool          asset_controls = true;
};

inline constexpr std::string_view kAnimeForward = "AnimeForward";
inline constexpr std::string_view kAnimeAlbedo = "AnimeAlbedo";
inline constexpr std::string_view kAnimeNormals = "AnimeNormals";
inline constexpr std::string_view kAnimeShadowCaster = "AnimeShadowCaster";
inline constexpr std::string_view kAnimeHairShadowCaster = "AnimeHairShadowCaster";

// Frame/instance/pass resources have CPU-owned layouts. MaterialData is generated
// exclusively from MaterialPass.bindings, using the asset storage ABI.
struct AnimeFrameData {
    math::mat4f inverse_vp, shadow_mvp;
    math::vec4f eye, light, shadow_settings, options;
};
struct AnimeInstanceData {
    math::mat4f mvp, model, normal_matrix;
};
struct AnimePassData {
    std::uint32_t mode;
    gfx::BindlessHandle shadow, hair_shadow, sampler;
};
struct AnimeDrawBindings {
    gfx::BindlessHandle frame, instance, material_data, pass_data;
    std::uint32_t instance_index, instance_stride, material_offset;
};
struct AnimeFullscreenData {
    gfx::BindlessHandle frame;
    math::vec4f settings;
    float scale;
    gfx::BindlessHandle texture0, texture1, texture2, texture3, sampler;
};

auto AnimeMaterialBindings() -> std::pmr::vector<asset::MaterialBinding>;
auto AnimeShaderProgram() -> gfx::ShaderProgramDesc;
// Resolve only texture-valued contract bindings, including defaults/placeholders.
// The returned Views must be declared to RenderGraph even though their
// bindless handles are already packed inside material_data.
auto AnimeMaterialViews(const asset::Material& material, const asset::MaterialPass& pass)
    -> std::vector<std::shared_ptr<gfx::TextureView>>;

class AnimeMaterialAdapter {
public:
    AnimeMaterialAdapter();
    auto Adapt(const asset::Material* source) const -> std::shared_ptr<asset::Material>;
private:
    std::shared_ptr<asset::RenderPipeline> m_Surface, m_Shadow;
};

class ToonRenderer {
public:
    explicit ToonRenderer(asset::ResourceLoadContext resources);
    // Materials must remain unchanged from Build through graph execution:
    // executors read their pass data using the sizes and dependencies declared here.
    auto Build(rg::RenderGraph& graph, std::span<const render::RenderDrawItem> items,
               const render::RenderView& view, std::uint32_t width, std::uint32_t height,
               math::vec3f light, float outline, math::vec3f bounds_min, math::vec3f bounds_max,
               RenderSettings settings = {}, bool diagnostics = false) -> FrameImages;

private:
    asset::ResourceLoadContext           m_Resources;
    std::shared_ptr<gfx::RenderPipeline> m_Fullscreen;
    std::shared_ptr<gfx::Sampler>        m_Sampler;
    std::shared_ptr<gfx::GPUBuffer>      m_FrameBuffer;
};
}  // namespace game::anime_viewer
