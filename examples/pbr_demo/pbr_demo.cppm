export module pbr_demo_game;
import engine;
export import preset.pbr;
import std;

export namespace game::pbr_demo {
using hitagi::preset::pbr::PbrMaterialDesc;
using hitagi::preset::pbr::CreatePbrMaterialParameters;
using hitagi::preset::pbr::kPbrForwardContract;
using PbrDemoRenderer = hitagi::preset::pbr::PbrRenderer;

auto DefaultSourceScenePath() -> std::filesystem::path;
auto DefaultCookedScenePath() -> std::filesystem::path;
auto FindDefaultSourceScenePath() -> std::filesystem::path;

void ConfigurePbrDemoScene(hitagi::asset::Scene& scene);

class PbrDemoGame final : public hitagi::core::RuntimeModule {
public:
    explicit PbrDemoGame(hitagi::Engine& engine, std::filesystem::path cooked_scene_path = DefaultCookedScenePath());

    void Tick() final;

private:
    void LoadSceneIfNeeded();
    auto BuildView(std::uint32_t width, std::uint32_t height) -> hitagi::render::RenderView;
    void CollectFrameData();

    hitagi::Engine&                                  m_Engine;
    std::filesystem::path                            m_CookedScenePath;
    std::shared_ptr<hitagi::asset::Scene>            m_Scene;
    std::pmr::vector<hitagi::render::RenderDrawItem> m_DrawItems;
    std::pmr::vector<hitagi::render::RenderLight>    m_Lights;
};

}
namespace game::pbr_demo {
using namespace hitagi;
bool ConfigurePbrDemoMaterial(const std::shared_ptr<asset::Material>& material) {
    if (!material) return false;
    const auto* pass = material->FindPass(kPbrForwardContract);
    return pass != nullptr && pass->pipeline != nullptr;
}

auto BuildRenderView(const asset::Camera& camera, const math::mat4f& camera_transform) noexcept -> render::RenderView {
    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;
    return {
        .camera_position = global_eye,
        .view            = math::look_at(global_eye, global_look_dir, global_up),
        .projection      = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip),
    };
}

auto BuildDefaultDemoView(float aspect) noexcept -> render::RenderView {
    const auto eye      = math::vec3f{0.0f, -3.0f, 1.6f};
    const auto look_dir = math::vec3f{0.0f, 1.0f, -0.35f};
    const auto up       = math::vec3f{0.0f, 0.0f, 1.0f};
    return {
        .camera_position = eye,
        .view            = math::look_at(eye, look_dir, up),
        .projection      = math::perspective(60.0_deg, aspect, 0.1f, 1000.0f),
    };
}

auto DefaultSourceScenePath() -> std::filesystem::path {
    return "assets/test/test.usda";
}

auto DefaultCookedScenePath() -> std::filesystem::path {
    return "assets/cooked/pbr_demo_scene.hcscene";
}

auto FindDefaultSourceScenePath() -> std::filesystem::path {
    static constexpr std::array candidates{
        std::string_view{"assets/DamagedHelmet/DamagedHelmet.usda"},
        std::string_view{"assets/DamagedHelmet/damaged_helmet.usda"},
        std::string_view{"assets/damaged_helmet/DamagedHelmet.usda"},
        std::string_view{"assets/test/pbr_texture_golden.usda"},
        std::string_view{"assets/test/test.usda"},
    };
    for (const auto candidate : candidates) {
        if (std::filesystem::exists(candidate)) return {candidate};
    }
    return DefaultSourceScenePath();
}

void ConfigurePbrDemoScene(asset::Scene& scene) {
    for (const auto entity : scene.GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>()) continue;
        auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (!mesh) continue;
        for (auto& sub_mesh : mesh->sub_meshes) {
            if (!sub_mesh.material) {
                throw std::runtime_error(std::format("PBR demo mesh '{}' has a submesh without material", mesh->GetName()));
            }
            if (!ConfigurePbrDemoMaterial(sub_mesh.material)) {
                throw std::runtime_error(std::format("PBR demo material '{}' is missing cooked PBRForward pass", sub_mesh.material->GetName()));
            }
        }
    }
}

PbrDemoGame::PbrDemoGame(hitagi::Engine& engine, std::filesystem::path cooked_scene_path)
    : RuntimeModule("PbrDemoGame"),
      m_Engine(engine),
      m_CookedScenePath(std::move(cooked_scene_path)) {}

void PbrDemoGame::LoadSceneIfNeeded() {
    if (m_Scene) return;
    if (!std::filesystem::exists(m_CookedScenePath)) {
        throw std::runtime_error(std::format("PBR demo cooked scene not found: {}", m_CookedScenePath.string()));
    }

    m_Scene = m_Engine.Assets().ImportScene(m_CookedScenePath);
    if (!m_Scene) {
        throw std::runtime_error(std::format("PBR demo failed to load cooked scene: {}", m_CookedScenePath.string()));
    }
    try {
        ConfigurePbrDemoScene(*m_Scene);
    } catch (const std::exception& error) {
        m_Scene.reset();
        throw std::runtime_error(std::format("PBR demo cooked scene is incompatible: {}", error.what()));
    }
    m_Logger->info("Loaded cooked scene: {}", m_CookedScenePath.string());
}

auto PbrDemoGame::BuildView(std::uint32_t width, std::uint32_t height) -> render::RenderView {
    const auto aspect = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
    if (!m_Scene) return BuildDefaultDemoView(aspect);

    if (auto camera_entity = m_Scene->GetCurrentCamera(); camera_entity && camera_entity.Has<asset::CameraComponent>() && camera_entity.Has<asset::Transform>()) {
        auto camera = camera_entity.Get<asset::CameraComponent>().camera;
        if (camera) {
            camera->parameters.aspect = aspect;
            return BuildRenderView(*camera, camera_entity.Get<asset::Transform>().world_matrix);
        }
    }
    return BuildDefaultDemoView(aspect);
}

void PbrDemoGame::CollectFrameData() {
    m_DrawItems.clear();
    m_Lights.clear();
    if (!m_Scene) return;

    m_Scene->Update(m_Engine.Jobs());
    m_DrawItems.reserve(m_Scene->GetMeshEntities().size());
    m_Lights.reserve(m_Scene->GetLightEntities().size());

    for (const auto entity : m_Scene->GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>() || !entity.Has<asset::Transform>()) continue;
        auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (!mesh || mesh->Empty()) continue;
        m_DrawItems.emplace_back(render::RenderDrawItem{
            .mesh      = std::move(mesh),
            .transform = entity.Get<asset::Transform>().world_matrix,
            .object_id = static_cast<std::uint32_t>(entity.GetId() + 1u),
        });
    }

    for (const auto entity : m_Scene->GetLightEntities()) {
        if (!entity || !entity.Has<asset::LightComponent>() || !entity.Has<asset::Transform>()) continue;
        auto light = entity.Get<asset::LightComponent>().light;
        if (!light) continue;
        m_Lights.emplace_back(render::RenderLight{
            .position  = (entity.Get<asset::Transform>().world_matrix * math::vec4f(light->parameters.position, 1.0f)).xyz,
            .color     = light->parameters.color,
            .intensity = std::max(light->parameters.intensity, 1.0f),
        });
    }
}

void PbrDemoGame::Tick() {
    LoadSceneIfNeeded();
    CollectFrameData();

    if (!m_Engine.App().WindowsMinimized()) {
        auto&      runtime = m_Engine.RenderRuntime();
        auto&      graph   = runtime.GetRenderGraph();
        const auto width   = runtime.GetSwapChain().GetWidth();
        const auto height  = runtime.GetSwapChain().GetHeight();

        auto output = graph.Create(gfx::TextureDesc{
            .name        = "PbrDemoGameOutput",
            .width       = width,
            .height      = height,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0.02f, 0.025f, 0.03f, 1.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc,
        });

        auto context = runtime.MakeContext();
        output       = m_Engine.Renderer().Render(
                                              context,
                                              render::RenderRequest{
                                                  .frame = render::RenderFrame{
                                                      .view       = BuildView(width, height),
                                                      .draw_items = m_DrawItems,
                                                      .lights     = m_Lights,
                                                  },
                                                  .target = output,
                                              })
                           .color;

        runtime.ToSwapChain(output);
    }
}

}  // namespace game::pbr_demo
