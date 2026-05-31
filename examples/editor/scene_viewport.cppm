module;

export module editor:scene_viewport;
import engine;
import :state;
import :command;

export namespace hitagi {
struct EditorViewportRay {
    math::vec3f origin;
    math::vec3f direction;
};

struct EditorPickResult {
    ecs::Entity entity;
    float       distance = std::numeric_limits<float>::max();
};

struct EditorViewportNavigationState {
    bool        orbiting          = false;
    math::vec3f orbit_pivot       = {};
    bool        has_orbit_pivot   = true;
};

struct EditorViewportNavigationInput {
    float       delta_time       = 0.0f;
    math::vec2f mouse_delta      = {};
    math::vec2f scroll_delta     = {};
    bool        viewport_hovered = false;
    bool        middle_down      = false;
};

auto BuildEditorViewportRay(
    const asset::Camera& camera,
    const math::mat4f&   camera_transform,
    math::vec2f          viewport_position,
    math::vec2f          viewport_size) noexcept -> EditorViewportRay;
auto PickEditorEntity(const asset::Scene& scene, const EditorViewportRay& ray, ecs::Entity ignored = {}) -> std::optional<EditorPickResult>;
auto ComputeEditorWorldXYGridMinorStep(
    math::vec3f camera_eye,
    float       horizontal_fov,
    float       aspect,
    float       image_height) noexcept -> float;
void ApplyEditorViewportNavigation(
    const asset::Camera::Parameters& camera_param,
    asset::Transform&                camera_transform,
    EditorViewportNavigationState&   state,
    const EditorViewportNavigationInput& input) noexcept;

class SceneViewPort : public core::RuntimeModule {
public:
    SceneViewPort(const Engine& engine, EditorState& state, EditorCommandStack& command_stack)
        : core::RuntimeModule("SceneViewPort"),
          m_Engine(engine),
          m_State(state),
          m_CommandStack(command_stack),
          m_GridPass(std::make_unique<render::passes::EditorGrid>(
              engine.RenderRuntime().GetRenderGraph().GetDevice(),
              engine.App().GetConfig().asset_root_path / "shaders/viewport_grid.hlsl")) {}

    void Tick() final;

    void SetScene(std::shared_ptr<asset::Scene> scene) noexcept;
    void FocusSelectedEntity();

    inline auto GetScene() const noexcept { return m_State.GetCurrentScene(); };

private:
    void MoveCamera() const;
    void RenderScene() const;
    void HandlePicking(math::vec2f image_min, math::vec2f image_size) const;
    void DrawTransformGizmo(math::vec2f image_min, math::vec2f image_size);
    void CommitTransformEdit(ecs::Entity entity, asset::Transform& transform);
    void DrawViewportOverlays(math::vec2f image_min, math::vec2f image_size) const;

    const Engine&                   m_Engine;
    EditorState&                    m_State;
    EditorCommandStack&             m_CommandStack;
    std::unique_ptr<render::passes::EditorGrid> m_GridPass;
    bool                            m_Open = true;
    ecs::Entity                     m_Camera;
    std::optional<asset::Transform> m_GizmoEditStart;
    mutable EditorViewportNavigationState m_CameraNavigation;
    bool                            m_DefaultLayoutDone = false;
};
}  // namespace hitagi
