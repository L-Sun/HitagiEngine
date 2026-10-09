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

class EditorViewportGridPass {
public:
    EditorViewportGridPass(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, const render::ShaderSource& shader);

    auto Build(render::RenderContext& context, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) -> rg::TextureHandle;

private:
    struct ViewportGridConstant {
        math::mat4f inv_proj_view;
        math::vec4f camera_pos;
        math::vec4f camera_forward;
        math::vec4f viewport_size_base_step_fade;
        math::vec4f clip_and_opacity;
    };

    struct ViewportGridBindlessInfo {
        gfx::BindlessHandle grid_constant;
    };

    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_PS;
    std::shared_ptr<gfx::RenderPipeline> m_Pipeline;
};

enum class EditorSelectionVisual : std::uint8_t {
    None,
    Selected,
    Hovered,
    Active,
};

struct EditorSelectionItem {
    ecs::Entity           entity;
    EditorSelectionVisual visual       = EditorSelectionVisual::Selected;
    std::uint32_t         selection_id = 1;
};

struct EditorSelectionDesc {
    bool                                  enabled = false;
    std::pmr::vector<EditorSelectionItem> items;

    math::Color selected_color = {1.0f, 0.72f, 0.10f, 1.0f};
    math::Color hovered_color  = {0.35f, 0.62f, 1.0f, 1.0f};
    math::Color occluded_color = {1.0f, 0.72f, 0.10f, 0.35f};

    float outline_width_px   = 2.0f;
    float highlight_strength = 0.15f;
    bool  show_occluded      = true;
};

struct EditorSelectionBuffers {
    rg::TextureHandle id;
    rg::TextureHandle visual;
    rg::TextureHandle depth;

    [[nodiscard]] constexpr auto Valid() const noexcept -> bool {
        return static_cast<bool>(id) && static_cast<bool>(visual) && static_cast<bool>(depth);
    }
};

class EditorSelectionMetadataPass {
public:
    EditorSelectionMetadataPass(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, render::ShaderSource shader);

    auto Build(
        render::RenderContext&     context,
        const render::RenderDrawState& draw_state,
        rg::GPUBufferHandle        frame_constant,
        const EditorSelectionDesc& desc,
        std::uint32_t              width,
        std::uint32_t              height) -> EditorSelectionBuffers;

private:
    struct InstanceConstant {
        math::mat4f                  model;
        std::uint32_t                selection_id = 0;
        std::uint32_t                visual_id    = 0;
        std::array<std::uint32_t, 2> padding      = {};
    };

    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle instance_constant;
    std::uint32_t instance_index = 0;
    std::uint32_t instance_stride = 0;
    };

    enum class Target : std::uint8_t {
        Id,
        Visual,
        Depth,
    };

    void EnsureResources();
    auto GetPipeline(Target target) -> std::shared_ptr<gfx::RenderPipeline>;
    void BuildTargetPass(
        render::RenderContext&              context,
        const render::RenderDrawState&       draw_state,
        rg::GPUBufferHandle                 frame_constant,
        rg::GPUBufferHandle                 instance_constant,
        rg::GPUBufferHandle                 bindless_info,
        rg::TextureHandle                   target,
        rg::TextureHandle                   depth_stencil,
        const std::shared_ptr<gfx::RenderPipeline>& pipeline,
        Target                              target_kind,
        std::span<const InstanceConstant>   selected_constants,
        std::span<const std::size_t>        selected_instances,
        std::uint64_t                      instance_stride,
        std::uint64_t                      bindless_stride);

    gfx::Device&                         m_Device;
    gfx::BindlessUtils&                  m_Bindings;
    const gfx::ShaderCompiler&           m_ShaderCompiler;
    render::ShaderSource                 m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_IdPS;
    std::shared_ptr<gfx::Shader>         m_VisualPS;
    std::shared_ptr<gfx::Shader>         m_DepthPS;
    std::shared_ptr<gfx::RenderPipeline> m_IdPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_VisualPipeline;
    std::shared_ptr<gfx::RenderPipeline> m_DepthPipeline;
};

class EditorSelectionOutlinePass {
public:
    EditorSelectionOutlinePass(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, render::ShaderSource shader);

    auto Build(
        render::RenderContext&          context,
        rg::TextureHandle               scene_color,
        rg::TextureHandle               scene_depth,
        const EditorSelectionBuffers&   selection,
        const EditorSelectionDesc&      desc,
        rg::SamplerHandle               sampler) -> rg::TextureHandle;

private:
    struct OutlineConstant {
        math::Color selected_color;
        math::Color hovered_color;
        math::Color occluded_color;
        math::vec4f params;
        math::vec4f viewport;
    };

    struct BindlessInfo {
        gfx::BindlessHandle outline_constant;
        gfx::BindlessHandle scene_color;
        gfx::BindlessHandle scene_depth;
        gfx::BindlessHandle selection_id;
        gfx::BindlessHandle selection_visual;
        gfx::BindlessHandle selection_depth;
        gfx::BindlessHandle sampler;
    };

    void EnsureResources(gfx::Format target_format);
    auto GetPipeline(gfx::Format target_format) -> std::shared_ptr<gfx::RenderPipeline>;

    gfx::Device&                         m_Device;
    gfx::BindlessUtils&                  m_Bindings;
    const gfx::ShaderCompiler&           m_ShaderCompiler;
    render::ShaderSource                 m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_PS;
    std::shared_ptr<gfx::RenderPipeline> m_Pipeline;
    gfx::Format                          m_TargetFormat = gfx::Format::UNKNOWN;
};

class EditorDeferredSelectionExtension final : public render::IDeferredRenderExtension {
public:
    EditorDeferredSelectionExtension(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, const render::ShaderSource& shader);

    void SetSelection(EditorSelectionDesc desc);

    void AfterGBuffer(
        render::RenderContext&                 context,
        const render::RenderView&               view,
        const render::DeferredRenderResources& resources,
        const render::DeferredDrawData&   draw_data) override;

    void AfterLighting(
        render::RenderContext&                 context,
        const render::RenderView&               view,
        render::DeferredRenderResources&       resources,
        const render::DeferredDrawData&   draw_data) override;

private:
    EditorSelectionDesc         m_Selection;
    EditorSelectionBuffers      m_Buffers;
    EditorSelectionMetadataPass m_MetadataPass;
    EditorSelectionOutlinePass  m_OutlinePass;
};

class SceneViewPort : public core::RuntimeModule {
public:
    SceneViewPort(const Engine& engine, EditorState& state, EditorCommandStack& command_stack);

    void Tick() final;

    void SetScene(const std::shared_ptr<asset::Scene>& scene) noexcept;
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
    std::shared_ptr<EditorDeferredSelectionExtension> m_SelectionExtension;
    std::unique_ptr<EditorViewportGridPass> m_GridPass;
    bool                            m_Open = true;
    ecs::Entity                     m_Camera;
    std::optional<asset::Transform> m_GizmoEditStart;
    mutable EditorViewportNavigationState m_CameraNavigation;
    bool                            m_DefaultLayoutDone = false;
};
}  // namespace hitagi
