module;

#include <imgui.h>
#include <range/v3/all.hpp>
#include <tracy/Tracy.hpp>

module editor;
import magic_enum;

using namespace hitagi;
using namespace hitagi::math;
using namespace hitagi::asset;

namespace {
constexpr auto kEmptyEntityPickExtents = vec3f{0.35f, 0.35f, 0.35f};
constexpr auto kEditorGizmoYellow      = IM_COL32(255, 201, 64, 235);
constexpr auto kEditorAxisX            = IM_COL32(238, 82, 82, 235);
constexpr auto kEditorAxisY            = IM_COL32(97, 210, 122, 235);
constexpr auto kEditorAxisZ            = IM_COL32(82, 150, 255, 235);

struct ViewportProjection {
    mat4f view_projection;
    vec2f image_min;
    vec2f image_size;
    vec3f camera_eye;
    vec3f camera_forward;
    vec3f camera_right;
    vec3f camera_up;
    float horizontal_fov;
    float aspect;
};

struct ViewportRenderSize {
    vec2u logical;
    vec2u physical;
};

constexpr auto kEditorViewportRenderScale = 0.75f;
constexpr auto kEditorViewportMaxWidth    = 2560u;
constexpr auto kEditorViewportMaxHeight   = 1440u;

auto RoundViewportDimension(std::uint32_t value) noexcept -> std::uint32_t {
    constexpr auto block_size = 16u;
    return std::max(block_size, ((value + block_size - 1u) / block_size) * block_size);
}

auto ComputeViewportRenderSize(vec2u logical_size) noexcept -> ViewportRenderSize {
    if (logical_size.x == 0 || logical_size.y == 0) {
        return {};
    }

    auto scale = std::clamp(kEditorViewportRenderScale, 0.25f, 1.0f);
    if (kEditorViewportMaxWidth > 0) {
        scale = std::min(scale, static_cast<float>(kEditorViewportMaxWidth) / static_cast<float>(logical_size.x));
    }
    if (kEditorViewportMaxHeight > 0) {
        scale = std::min(scale, static_cast<float>(kEditorViewportMaxHeight) / static_cast<float>(logical_size.y));
    }
    scale = std::clamp(scale, 0.25f, 1.0f);

    return {
        .logical  = logical_size,
        .physical = {
            RoundViewportDimension(static_cast<std::uint32_t>(std::ceil(static_cast<float>(logical_size.x) * scale))),
            RoundViewportDimension(static_cast<std::uint32_t>(std::ceil(static_cast<float>(logical_size.y) * scale))),
        },
    };
}

auto EntityPickBounds(ecs::Entity entity) noexcept -> AABBf {
    if (!entity || !entity.Has<Transform>()) return {};

    const auto& transform = entity.Get<Transform>();
    if (entity.Has<MeshComponent>()) {
        const auto mesh = entity.Get<MeshComponent>().mesh;
        if (mesh && mesh->aabb.Valid()) return transform_aabb(transform.world_matrix, mesh->aabb);
    }

    const auto center = get_translation(transform.world_matrix);
    return AABBf{
        .min_point = center - kEmptyEntityPickExtents,
        .max_point = center + kEmptyEntityPickExtents,
    };
}

auto IsEditorPickCandidate(ecs::Entity entity, ecs::Entity root) noexcept -> bool {
    if (!entity || entity == root || !entity.Has<Transform>() || !entity.Has<MeshComponent>()) return false;

    const auto mesh = entity.Get<MeshComponent>().mesh;
    return mesh != nullptr && !mesh->Empty();
}

void VisitEntityTree(ecs::Entity entity, const std::function<void(ecs::Entity)>& visitor) {
    if (!entity) return;
    visitor(entity);
    if (!entity.Has<RelationShip>()) return;

    for (const auto child : entity.Get<RelationShip>().GetChildren()) {
        VisitEntityTree(child, visitor);
    }
}

auto SnapValue(float value, float step) noexcept -> float {
    if (step <= 0.0f) return value;
    return std::round(value / step) * step;
}

auto SnapVector(vec3f value, float step) noexcept -> vec3f {
    return {
        SnapValue(value.x, step),
        SnapValue(value.y, step),
        SnapValue(value.z, step),
    };
}

auto BuildViewportProjection(const asset::Camera& camera, const mat4f& camera_transform, vec2f image_min, vec2f image_size) noexcept -> ViewportProjection {
    const vec3f global_eye      = (camera_transform * vec4f(camera.parameters.eye, 1.0f)).xyz;
    const vec3f global_look_dir = (camera_transform * vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const vec3f global_up       = (camera_transform * vec4f(camera.parameters.up, 0.0f)).xyz;
    const vec3f camera_forward  = normalize(global_look_dir);
    const vec3f camera_right    = normalize(cross(camera_forward, normalize(global_up)));
    const vec3f camera_up       = normalize(cross(camera_right, camera_forward));
    const auto  view            = look_at(global_eye, global_look_dir, global_up);
    const auto  projection      = perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    return {
        .view_projection = projection * view,
        .image_min       = image_min,
        .image_size      = image_size,
        .camera_eye      = global_eye,
        .camera_forward  = camera_forward,
        .camera_right    = camera_right,
        .camera_up       = camera_up,
        .horizontal_fov  = camera.parameters.horizontal_fov,
        .aspect          = camera.parameters.aspect,
    };
}

auto EditorEntityObjectId(ecs::Entity entity) noexcept -> std::uint32_t {
    return entity ? static_cast<std::uint32_t>(entity.GetId() + 1u) : 0u;
}

auto BuildRenderView(const asset::Camera& camera, const mat4f& camera_transform) noexcept -> render::RenderView {
    const vec3f global_eye      = (camera_transform * vec4f(camera.parameters.eye, 1.0f)).xyz;
    const vec3f global_look_dir = (camera_transform * vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const vec3f global_up       = (camera_transform * vec4f(camera.parameters.up, 0.0f)).xyz;
    return {
        .camera_position = global_eye,
        .view            = look_at(global_eye, global_look_dir, global_up),
        .projection      = perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip),
    };
}

void CollectSceneRenderFrameData(
    asset::Scene&                             scene,
    std::pmr::vector<render::RenderDrawItem>& draw_items,
    std::pmr::vector<render::RenderLight>&    lights) {
    draw_items.clear();
    lights.clear();
    draw_items.reserve(scene.GetMeshEntities().size());
    lights.reserve(scene.GetLightEntities().size());

    for (const auto entity : scene.GetMeshEntities()) {
        if (!entity || !entity.Has<MeshComponent>() || !entity.Has<Transform>()) continue;
        const auto mesh = entity.Get<MeshComponent>().mesh;
        if (!mesh || mesh->Empty()) continue;
        draw_items.emplace_back(render::RenderDrawItem{
            .mesh      = mesh,
            .transform = entity.Get<Transform>().world_matrix,
            .object_id = EditorEntityObjectId(entity),
        });
    }

    for (const auto entity : scene.GetLightEntities()) {
        if (!entity || !entity.Has<LightComponent>() || !entity.Has<Transform>()) continue;
        const auto light = entity.Get<LightComponent>().light;
        if (!light) continue;
        lights.emplace_back(render::RenderLight{
            .position  = (entity.Get<Transform>().world_matrix * vec4f(light->parameters.position, 1.0f)).xyz,
            .color     = light->parameters.color,
            .intensity = light->parameters.intensity,
        });
    }
}

auto ClipToScreen(const ViewportProjection& projection, vec4f clip) noexcept -> ImVec2 {
    clip /= clip.w;
    return ImVec2{
        projection.image_min.x + (clip.x * 0.5f + 0.5f) * projection.image_size.x,
        projection.image_min.y + (0.5f - clip.y * 0.5f) * projection.image_size.y,
    };
}

auto ProjectWorldPoint(const ViewportProjection& projection, vec3f world) noexcept -> std::optional<ImVec2> {
    const auto clip = projection.view_projection * vec4f{world, 1.0f};
    if (clip.w <= 1e-5f) return std::nullopt;
    return ClipToScreen(projection, clip);
}

auto ProjectWorldLine(const ViewportProjection& projection, vec3f from, vec3f to) noexcept -> std::optional<std::pair<ImVec2, ImVec2>> {
    auto a = projection.view_projection * vec4f{from, 1.0f};
    auto b = projection.view_projection * vec4f{to, 1.0f};

    constexpr auto min_w = 1e-4f;
    if (a.w <= min_w && b.w <= min_w) return std::nullopt;
    if (a.w <= min_w || b.w <= min_w) {
        const auto t = (min_w - a.w) / (b.w - a.w);
        if (a.w <= min_w) {
            a = a + (b - a) * t;
        } else {
            b = a + (b - a) * t;
        }
    }

    return std::pair{ClipToScreen(projection, a), ClipToScreen(projection, b)};
}

void DrawProjectedLine(ImDrawList* draw_list, const ViewportProjection& projection, vec3f from, vec3f to, ImU32 color, float thickness = 1.0f) {
    const auto line = ProjectWorldLine(projection, from, to);
    if (!line) return;
    draw_list->AddLine(line->first, line->second, color, thickness);
}

void DrawProjectedCircle(
    ImDrawList*               draw_list,
    const ViewportProjection& projection,
    vec3f                     center,
    vec3f                     axis_a,
    vec3f                     axis_b,
    float                     radius,
    ImU32                     color) {
    constexpr auto        segment_count = 72;
    std::optional<ImVec2> previous;

    for (int i = 0; i <= segment_count; ++i) {
        const auto t      = static_cast<float>(i) / static_cast<float>(segment_count) * std::numbers::pi_v<float> * 2.0f;
        const auto point  = center + axis_a * (std::cos(t) * radius) + axis_b * (std::sin(t) * radius);
        const auto screen = ProjectWorldPoint(projection, point);
        if (previous && screen) draw_list->AddLine(*previous, *screen, color, 1.5f);
        previous = screen;
    }
}

void DrawSelectedAxis(ImDrawList* draw_list, const ViewportProjection& projection, const Transform& transform, const AABBf& bounds) {
    const auto center = bounds.Valid() ? bounds.Center() : get_translation(transform.world_matrix);
    const auto length = std::max(bounds.Valid() ? bounds.Extents().norm() * 1.35f : 1.0f, 0.75f);

    DrawProjectedLine(draw_list, projection, center, center + get_right(transform.world_matrix) * length, kEditorAxisX, 2.5f);
    DrawProjectedLine(draw_list, projection, center, center + get_up(transform.world_matrix) * length, kEditorAxisY, 2.5f);
    DrawProjectedLine(draw_list, projection, center, center + get_forward(transform.world_matrix) * length, kEditorAxisZ, 2.5f);

    if (const auto c = ProjectWorldPoint(projection, center)) {
        draw_list->AddCircleFilled(*c, 4.0f, kEditorGizmoYellow);
    }
}

void DrawSelectedRotationOrbit(ImDrawList* draw_list, const ViewportProjection& projection, const Transform& transform, const AABBf& bounds) {
    const auto center = bounds.Valid() ? bounds.Center() : get_translation(transform.world_matrix);
    const auto radius = std::max(bounds.Valid() ? bounds.Extents().norm() * 1.6f : 1.0f, 0.9f);
    const auto x      = get_right(transform.world_matrix);
    const auto y      = get_up(transform.world_matrix);
    const auto z      = get_forward(transform.world_matrix);

    DrawProjectedCircle(draw_list, projection, center, y, z, radius, kEditorAxisX);
    DrawProjectedCircle(draw_list, projection, center, x, z, radius, kEditorAxisY);
    DrawProjectedCircle(draw_list, projection, center, x, y, radius, kEditorAxisZ);
}

void DrawViewportOrientationGizmo(ImDrawList* draw_list, vec2f image_min, vec2f image_size) {
    const auto origin = ImVec2{image_min.x + 42.0f, image_min.y + image_size.y - 38.0f};
    const auto x      = ImVec2{origin.x + 28.0f, origin.y};
    const auto y      = ImVec2{origin.x + 20.0f, origin.y - 16.0f};
    const auto z      = ImVec2{origin.x, origin.y - 30.0f};

    draw_list->AddCircleFilled(origin, 3.5f, IM_COL32(230, 235, 242, 210));
    draw_list->AddLine(origin, x, kEditorAxisX, 2.5f);
    draw_list->AddLine(origin, y, kEditorAxisY, 2.5f);
    draw_list->AddLine(origin, z, kEditorAxisZ, 2.5f);
    draw_list->AddText(ImVec2{x.x + 4.0f, x.y - 7.0f}, kEditorAxisX, "X");
    draw_list->AddText(ImVec2{y.x + 4.0f, y.y - 12.0f}, kEditorAxisY, "Y");
    draw_list->AddText(ImVec2{z.x - 4.0f, z.y - 18.0f}, kEditorAxisZ, "Z");
}

auto NiceGridStep(float raw_step) noexcept -> float {
    if (raw_step <= 0.0f) return 1.0f;

    const auto exponent = std::floor(std::log10(raw_step));
    const auto scale    = std::pow(10.0f, exponent);
    const auto mantissa = raw_step / scale;

    if (mantissa <= 1.0f) return scale;
    if (mantissa <= 2.0f) return 2.0f * scale;
    if (mantissa <= 5.0f) return 5.0f * scale;
    return 10.0f * scale;
}

auto ComputeEditorWorldXYGridRawStep(
    math::vec3f camera_eye,
    float       horizontal_fov,
    float       aspect,
    float       image_height) noexcept -> float {
    constexpr auto target_minor_pixels  = 42.0f;
    const auto     distance_from_origin = camera_eye.norm();
    const auto     distance_to_plane    = std::abs(camera_eye.z);
    const auto     density_distance     = std::max({distance_from_origin, distance_to_plane, 1.0f});
    const auto     vertical_fov         = 2.0f * std::atan(std::tan(horizontal_fov * 0.5f) / aspect);
    const auto     world_per_pixel      = 2.0f * std::tan(vertical_fov * 0.5f) * density_distance / std::max(image_height, 1.0f);
    return std::clamp(world_per_pixel * target_minor_pixels, 0.1f, 100.0f);
}
}  // namespace

auto hitagi::ComputeEditorWorldXYGridMinorStep(
    math::vec3f camera_eye,
    float       horizontal_fov,
    float       aspect,
    float       image_height) noexcept -> float {
    return std::clamp(NiceGridStep(ComputeEditorWorldXYGridRawStep(camera_eye, horizontal_fov, aspect, image_height)), 0.1f, 100.0f);
}

EditorViewportGridPass::EditorViewportGridPass(gfx::Device& device, const render::ShaderSource& shader) {
    m_VS       = device.CreateShader({
        .name        = "viewport-grid-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = shader.code,
        .path        = shader.path,
    });
    m_PS       = device.CreateShader({
        .name        = "viewport-grid-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = shader.code,
        .path        = shader.path,
    });
    m_Pipeline = device.CreateRenderPipeline(
        {
            .name           = "viewport-grid",
            .assembly_state = {
                .primitive = gfx::PrimitiveTopology::TriangleList,
            },
            .rasterization_state = {
                .cull_mode               = gfx::CullMode::None,
                .front_counter_clockwise = false,
            },
            .blend_state = {
                .blend_enable           = true,
                .src_color_blend_factor = gfx::BlendFactor::SrcAlpha,
                .dst_color_blend_factor = gfx::BlendFactor::InvSrcAlpha,
                .color_blend_op         = gfx::BlendOp::Add,
                .src_alpha_blend_factor = gfx::BlendFactor::One,
                .dst_alpha_blend_factor = gfx::BlendFactor::InvSrcAlpha,
                .alpha_blend_op         = gfx::BlendOp::Add,
            },
            .render_format = gfx::Format::R8G8B8A8_UNORM,
        },
        {m_VS, m_PS});
}

auto EditorViewportGridPass::Build(render::RenderContext& context, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) || !m_Pipeline) return target;
    const auto output = render_graph.MoveFrom(target, std::format("Viewport Grid Target {}", render_graph.GetFrameIndex()));

    const auto        camera_parameters = camera.parameters;
    const math::vec3f global_eye        = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir   = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up         = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;
    const auto        view              = math::look_at(global_eye, global_look_dir, global_up);
    const auto        projection        = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto        forward           = math::normalize(global_look_dir);
    const auto        forward_z         = std::abs(forward.z);
    const auto        below_dist        = std::abs(global_eye.z);
    const auto        look_dist         = forward_z > 1e-4f ? below_dist / forward_z : below_dist * 10.0f;
    const auto        grid_distance     = std::max(std::lerp(look_dist, below_dist, 1.0f - forward_z), 0.1f);
    const auto        base_step         = std::clamp(std::pow(10.0f, std::floor(std::log10(grid_distance))), 0.1f, 100.0f);
    const auto        next_step         = std::clamp(base_step * 10.0f, 0.1f, 1000.0f);
    const auto        fade              = std::clamp((grid_distance - base_step) / std::max(next_step - base_step, 1e-5f), 0.0f, 1.0f);

    const auto target_desc          = render_graph.GetResourceDesc(output);
    const auto grid_constant_handle = render_graph.Create(
        {
            .name   = "viewport_grid_constant",
            .size   = utils::align(sizeof(ViewportGridConstant), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
            .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "viewport_grid_constant");
    const auto bindless_info_handle = render_graph.Create(
        {
            .name   = "viewport_grid_bindless",
            .size   = utils::align(sizeof(ViewportGridBindlessInfo), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
            .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "viewport_grid_bindless");
    const auto pipeline = m_Pipeline;

    rg::RenderPassBuilder builder(render_graph);
    builder.SetName(std::format("ViewportGridPass-{}", render_graph.GetFrameIndex()));
    const auto grid_constant_access = builder.Read(grid_constant_handle, {.offset = 0, .element_size = sizeof(ViewportGridConstant), .element_count = 1});
    const auto bindless_info_access = builder.Read(bindless_info_handle, {.offset = 0, .element_size = sizeof(ViewportGridBindlessInfo), .element_count = 1});
    builder.SetRenderTarget(output, false);
    builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        pass.Resolve(grid_constant_access).GetMappedSpan<ViewportGridConstant>().front() = ViewportGridConstant{
            .inv_proj_view                = math::inverse(projection * view),
            .camera_pos                   = {global_eye, 1.0f},
            .camera_forward               = {forward, 0.0f},
            .viewport_size_base_step_fade = {static_cast<float>(target_desc.width), static_cast<float>(target_desc.height), base_step, fade},
            .clip_and_opacity             = {camera_parameters.far_clip, 0.65f, 0.0f, 0.0f},
        };

        auto bindless_infos                  = pass.Resolve(bindless_info_access).GetMappedSpan<ViewportGridBindlessInfo>();
        bindless_infos.front().grid_constant = pass.Resolve(grid_constant_access).GetBindlessHandle();

        const auto& render_target = pass.Resolve(output);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({
            .handle = pass.Resolve(bindless_info_access).GetBindlessHandle(),
        });
        cmd.Draw(3);
    });
    builder.Finish();
    return output;
}

EditorSelectionMetadataPass::EditorSelectionMetadataPass(gfx::Device& device, render::ShaderSource shader)
    : m_Device(device),
      m_Shader(std::move(shader)) {}

void EditorSelectionMetadataPass::EnsureResources() {
    if (m_IdPipeline && m_VisualPipeline && m_DepthPipeline) return;

    m_VS       = m_Device.CreateShader({
        .name        = "editor-selection-mask-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSSelectionMaskMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_IdPS     = m_Device.CreateShader({
        .name        = "editor-selection-id-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionIdMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_VisualPS = m_Device.CreateShader({
        .name        = "editor-selection-visual-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionVisualMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_DepthPS  = m_Device.CreateShader({
        .name        = "editor-selection-depth-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionDepthMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });

    const auto vertex_layout = m_Device.GetShaderCompiler().ExtractVertexLayout(m_VS->GetDesc());

    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& ps, gfx::Format format, bool depth_write) {
        return m_Device.CreateRenderPipeline(
            {
                .name                = std::pmr::string(name),
                .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
                .vertex_input_layout = vertex_layout,
                .rasterization_state = {.cull_mode = gfx::CullMode::None},
                .depth_stencil_state = {
                    .depth_test_enable  = true,
                    .depth_write_enable = depth_write,
                    .depth_compare_op   = gfx::CompareOp::LessEqual,
                },
                .render_format        = format,
                .depth_stencil_format = gfx::Format::D32_FLOAT,
            },
            {m_VS, ps});
    };

    m_IdPipeline     = make_pipeline("editor-selection-id", m_IdPS, gfx::Format::R32_UINT, true);
    m_VisualPipeline = make_pipeline("editor-selection-visual", m_VisualPS, gfx::Format::R32_UINT, false);
    m_DepthPipeline  = make_pipeline("editor-selection-depth", m_DepthPS, gfx::Format::R32_FLOAT, false);
}

auto EditorSelectionMetadataPass::GetPipeline(Target target) -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources();
    switch (target) {
        case Target::Id:
            return m_IdPipeline;
        case Target::Visual:
            return m_VisualPipeline;
        case Target::Depth:
            return m_DepthPipeline;
    }
    return {};
}

auto EditorSelectionMetadataPass::Build(
    render::RenderContext&         context,
    const render::RenderDrawState& draw_state,
    rg::GPUBufferHandle            frame_constant,
    const EditorSelectionDesc&     desc,
    std::uint32_t                  width,
    std::uint32_t                  height) -> EditorSelectionBuffers {
    auto& render_graph = context.graph;
    if (!desc.enabled || desc.items.empty() || draw_state.instance_infos.empty() || !render_graph.IsValid(frame_constant)) return {};

    std::pmr::vector<EditorSelectionItem> selected_items;
    selected_items.reserve(desc.items.size());
    for (const auto& item : desc.items) {
        if (item.entity && item.selection_id != 0) selected_items.emplace_back(item);
    }
    if (selected_items.empty()) return {};

    std::pmr::vector<std::size_t> selected_instances;
    for (std::size_t i = 0; i < draw_state.instance_infos.size(); ++i) {
        const auto object_id = draw_state.instance_infos[i].object_id;
        const auto item_it   = ranges::find_if(selected_items, [object_id](const EditorSelectionItem& item) {
            return EditorEntityObjectId(item.entity) == object_id;
        });
        if (item_it != selected_items.end()) selected_instances.emplace_back(i);
    }
    if (selected_instances.empty()) return {};

    const auto id              = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_id_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_UINT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto visual          = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_visual_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_UINT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto selection_depth = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_depth_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_FLOAT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto depth_stencil   = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_depth_stencil_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::D32_FLOAT,
        .clear_value = gfx::ClearDepthStencil{.depth = 1.0f, .stencil = 0},
        .usages      = gfx::TextureUsageFlags::DepthStencil,
    });

    auto instance_constant = render_graph.Create({
        .name   = "editor_selection_instance_constant",
        .size   = sizeof(InstanceConstant) * std::max<std::size_t>(1, selected_instances.size()),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });
    auto bindless_info     = render_graph.Create({
        .name   = "editor_selection_bindless_infos",
        .size   = sizeof(BindlessInfo) * std::max<std::size_t>(1, selected_instances.size()),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    std::pmr::vector<InstanceConstant> selected_constants;
    selected_constants.resize(selected_instances.size());
    for (std::size_t selection_index = 0; selection_index < selected_instances.size(); ++selection_index) {
        const auto& instance                = draw_state.instance_infos[selected_instances[selection_index]];
        const auto  item_it                 = ranges::find_if(selected_items, [object_id = instance.object_id](const EditorSelectionItem& item) {
            return EditorEntityObjectId(item.entity) == object_id;
        });
        const auto  visual_id               = item_it != selected_items.end()
                                                  ? static_cast<std::uint32_t>(item_it->visual)
                                                  : static_cast<std::uint32_t>(EditorSelectionVisual::Selected);
        selected_constants[selection_index] = {
            .model        = instance.instance_data.model,
            .selection_id = item_it != selected_items.end() ? item_it->selection_id : 1u,
            .visual_id    = visual_id,
        };
    }

    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, id, depth_stencil, GetPipeline(Target::Id), Target::Id, selected_constants, selected_instances, sizeof(InstanceConstant), sizeof(BindlessInfo));
    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, visual, depth_stencil, GetPipeline(Target::Visual), Target::Visual, selected_constants, selected_instances, sizeof(InstanceConstant), sizeof(BindlessInfo));
    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, selection_depth, depth_stencil, GetPipeline(Target::Depth), Target::Depth, selected_constants, selected_instances, sizeof(InstanceConstant), sizeof(BindlessInfo));

    return {
        .id     = id,
        .visual = visual,
        .depth  = selection_depth,
    };
}

void EditorSelectionMetadataPass::BuildTargetPass(
    render::RenderContext&               context,
    const render::RenderDrawState&       draw_state,
    rg::GPUBufferHandle                  frame_constant,
    rg::GPUBufferHandle                  instance_constant,
    rg::GPUBufferHandle                  bindless_info,
    rg::TextureHandle                    target,
    rg::TextureHandle                    depth_stencil,
    std::shared_ptr<gfx::RenderPipeline> pipeline,
    Target                               target_kind,
    std::span<const InstanceConstant>    selected_constants,
    std::span<const std::size_t>         selected_instances,
    std::uint64_t                        instance_stride,
    std::uint64_t                        bindless_stride) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) || !render_graph.IsValid(depth_stencil) || !pipeline) return;

    auto pass_name = std::format(
        "EditorSelection{}Pass-{}",
        target_kind == Target::Id ? "Id" : target_kind == Target::Visual ? "Visual"
                                                                         : "Depth",
        render_graph.GetFrameIndex());

    rg::RenderPassBuilder builder(render_graph);
    builder.SetName(pass_name);
    builder.SetRenderTarget(target, true);
    const auto frame_constant_access    = builder.Read(frame_constant, {.offset = 0, .element_size = sizeof(render::FrameConstant), .element_count = 1}, gfx::PipelineStage::VertexShader);
    const auto instance_constant_access = builder.Read(instance_constant, {.offset = 0, .element_size = sizeof(InstanceConstant), .element_count = std::max<std::size_t>(1, selected_instances.size()), .element_stride = instance_stride}, gfx::PipelineStage::VertexShader);
    const auto bindless_info_access     = builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(BindlessInfo), .element_count = std::max<std::size_t>(1, selected_instances.size()), .element_stride = bindless_stride}, gfx::PipelineStage::All);

    if (target_kind == Target::Id) {
        builder.SetDepthStencil(depth_stencil, true);
    } else {
        builder.ReadDepthStencil(depth_stencil);
    }

    for (const auto& [_, mesh_info] : draw_state.mesh_infos) {
        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            const auto handle = mesh_info.vertices[attr];
            if (handle) builder.ReadAsVertices(handle);
        });
        builder.ReadAsIndices(mesh_info.indices);
    }

    std::pmr::vector<std::size_t>      selected_instances_copy{selected_instances.begin(), selected_instances.end()};
    std::pmr::vector<InstanceConstant> selected_constants_copy{selected_constants.begin(), selected_constants.end()};
    builder.SetExecutor([&draw_state, frame_constant_access, instance_constant_access, bindless_info_access, target, pipeline, selected_instances_copy, selected_constants_copy](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        auto instance_constants = pass.Resolve(instance_constant_access).GetMappedSpan<InstanceConstant>();
        auto bindless_infos     = pass.Resolve(bindless_info_access).GetMappedSpan<BindlessInfo>();
        for (std::size_t i = 0; i < selected_instances_copy.size(); ++i) {
            instance_constants[i] = selected_constants_copy[i];
            bindless_infos[i]     = {
                .frame_constant    = pass.Resolve(frame_constant_access).GetBindlessHandle(),
                .instance_constant = pass.Resolve(instance_constant_access).GetBindlessHandle(),
                .instance_index    = static_cast<std::uint32_t>(i),
                .instance_stride   = static_cast<std::uint32_t>(pass.Resolve(instance_constant_access).GetDesc().element_stride),
            };
        }

        const auto& render_target = pass.Resolve(target);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });

        auto& pipeline_ref = *pipeline;
        cmd.SetPipeline(pipeline_ref);

        for (std::size_t selection_index = 0; selection_index < selected_instances_copy.size(); ++selection_index) {
            const auto& instance_info = draw_state.instance_infos[selected_instances_copy[selection_index]];
            const auto  mesh_info     = draw_state.mesh_infos.at(instance_info.mesh.get());

            cmd.PushBindlessMetaInfo({
                .handle        = pass.Resolve(bindless_info_access).GetBindlessHandle(),
                .record_index  = static_cast<std::uint32_t>(selection_index),
                .record_stride = static_cast<std::uint32_t>(pass.Resolve(bindless_info_access).GetDesc().element_stride),
            });
            for (const auto& vertex_attr : pipeline_ref.GetDesc().vertex_input_layout) {
                auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                auto attr_handle = mesh_info.vertices[mesh_attr];
                if (attr_handle) {
                    cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                }
            }
            cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0, mesh_info.index_format);
            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
            }
        }
    });
    builder.Finish();
}

EditorSelectionOutlinePass::EditorSelectionOutlinePass(gfx::Device& device, render::ShaderSource shader)
    : m_Device(device),
      m_Shader(std::move(shader)) {}

void EditorSelectionOutlinePass::EnsureResources(gfx::Format target_format) {
    if (m_Pipeline && m_TargetFormat == target_format) return;

    if (m_VS == nullptr) {
        m_VS = m_Device.CreateShader({
            .name        = "selection-outline-vs",
            .type        = gfx::ShaderType::Vertex,
            .entry       = "VSFullscreenMain",
            .source_code = m_Shader.code,
            .path        = m_Shader.path,
        });
        m_PS = m_Device.CreateShader({
            .name        = "selection-outline-ps",
            .type        = gfx::ShaderType::Pixel,
            .entry       = "PSSelectionOutlineMain",
            .source_code = m_Shader.code,
            .path        = m_Shader.path,
        });
    }

    m_Pipeline = m_Device.CreateRenderPipeline(
        {
            .name                = "selection-outline",
            .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .render_format       = target_format,
        },
        {m_VS, m_PS});
    m_TargetFormat = target_format;
}

auto EditorSelectionOutlinePass::GetPipeline(gfx::Format target_format) -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources(target_format);
    return m_Pipeline;
}

auto EditorSelectionOutlinePass::Build(
    render::RenderContext&        context,
    rg::TextureHandle             scene_color,
    rg::TextureHandle             scene_depth,
    const EditorSelectionBuffers& selection,
    const EditorSelectionDesc&    desc,
    rg::SamplerHandle             sampler) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!selection.Valid() ||
        !render_graph.IsValid(scene_color) ||
        !render_graph.IsValid(scene_depth) ||
        !render_graph.IsValid(selection.id) ||
        !render_graph.IsValid(selection.visual) ||
        !render_graph.IsValid(selection.depth) ||
        !render_graph.IsValid(sampler)) {
        return scene_color;
    }

    auto target_desc         = render_graph.GetResourceDesc(scene_color);
    target_desc.name         = std::pmr::string(std::format("SelectionOutlineOutput-{}", render_graph.GetFrameIndex()));
    target_desc.clear_value  = std::nullopt;
    const auto output        = render_graph.Create(target_desc);
    const auto pipeline      = GetPipeline(target_desc.format);
    const auto constant      = render_graph.Create({
        .name   = "selection_outline_constant",
        .size   = utils::align(sizeof(OutlineConstant), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });
    const auto bindless_info = render_graph.Create({
        .name   = "selection_outline_bindless_info",
        .size   = utils::align(sizeof(BindlessInfo), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    rg::RenderPassBuilder pass_builder(render_graph);
    pass_builder.SetName(std::format("SelectionOutlinePass-{}", render_graph.GetFrameIndex()));
    pass_builder.SetRenderTarget(output, false);
    const auto scene_color_access   = pass_builder.Read(scene_color, {}, gfx::PipelineStage::PixelShader);
    const auto scene_depth_access   = pass_builder.Read(scene_depth, {}, gfx::PipelineStage::PixelShader);
    const auto id_access            = pass_builder.Read(selection.id, {}, gfx::PipelineStage::PixelShader);
    const auto visual_access        = pass_builder.Read(selection.visual, {}, gfx::PipelineStage::PixelShader);
    const auto depth_access         = pass_builder.Read(selection.depth, {}, gfx::PipelineStage::PixelShader);
    const auto constant_access      = pass_builder.Read(constant, {.offset = 0, .element_size = sizeof(OutlineConstant), .element_count = 1}, gfx::PipelineStage::PixelShader);
    const auto bindless_info_access = pass_builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(BindlessInfo), .element_count = 1}, gfx::PipelineStage::PixelShader);
    pass_builder.AddSampler(sampler);
    pass_builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        pass.Resolve(constant_access).GetMappedSpan<OutlineConstant>().front() = {
            .selected_color = desc.selected_color,
            .hovered_color  = desc.hovered_color,
            .occluded_color = desc.occluded_color,
            .params         = {
                desc.outline_width_px,
                desc.highlight_strength,
                desc.show_occluded ? 1.0f : 0.0f,
                0.015f,
            },
            .viewport = {
                static_cast<float>(target_desc.width),
                static_cast<float>(target_desc.height),
                1.0f / static_cast<float>(target_desc.width),
                1.0f / static_cast<float>(target_desc.height),
            },
        };
        pass.Resolve(bindless_info_access).GetMappedSpan<BindlessInfo>().front() = {
            .outline_constant = pass.Resolve(constant_access).GetBindlessHandle(),
            .scene_color      = pass.Resolve(scene_color_access).GetBindlessHandle(),
            .scene_depth      = pass.Resolve(scene_depth_access).GetBindlessHandle(),
            .selection_id     = pass.Resolve(id_access).GetBindlessHandle(),
            .selection_visual = pass.Resolve(visual_access).GetBindlessHandle(),
            .selection_depth  = pass.Resolve(depth_access).GetBindlessHandle(),
            .sampler          = pass.Resolve(sampler).GetBindlessHandle(),
        };

        auto&       cmd           = pass.GetCmd();
        const auto& render_target = pass.Resolve(output);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({
            .handle = pass.Resolve(bindless_info_access).GetBindlessHandle(),
        });
        cmd.Draw(3);
    });
    pass_builder.Finish();

    return output;
}

EditorDeferredSelectionExtension::EditorDeferredSelectionExtension(gfx::Device& device, const render::ShaderSource& shader)
    : m_MetadataPass(device, shader),
      m_OutlinePass(device, shader) {}

void EditorDeferredSelectionExtension::SetSelection(EditorSelectionDesc desc) {
    m_Selection = std::move(desc);
    m_Buffers   = {};
}

void EditorDeferredSelectionExtension::AfterGBuffer(
    render::RenderContext& context,
    const render::RenderView&,
    const render::DeferredRenderResources& resources,
    const render::DeferredDrawData&        draw_data) {
    if (draw_data.draw_state == nullptr) return;
    m_Buffers = m_MetadataPass.Build(
        context,
        *draw_data.draw_state,
        resources.frame_constant,
        m_Selection,
        resources.width,
        resources.height);
}

void EditorDeferredSelectionExtension::AfterLighting(
    render::RenderContext& context,
    const render::RenderView&,
    render::DeferredRenderResources& resources,
    const render::DeferredDrawData&) {
    if (!m_Buffers.Valid()) return;
    resources.color = m_OutlinePass.Build(
        context,
        resources.color,
        resources.linear_depth,
        m_Buffers,
        m_Selection,
        resources.sampler);
}

auto hitagi::BuildEditorViewportRay(
    const asset::Camera& camera,
    const math::mat4f&   camera_transform,
    math::vec2f          viewport_position,
    math::vec2f          viewport_size) noexcept -> EditorViewportRay {
    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
        return {};
    }

    const vec3f global_eye      = (camera_transform * vec4f(camera.parameters.eye, 1.0f)).xyz;
    const vec3f global_look_dir = (camera_transform * vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const vec3f global_up       = (camera_transform * vec4f(camera.parameters.up, 0.0f)).xyz;
    const auto  view            = look_at(global_eye, global_look_dir, global_up);
    const auto  projection      = perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto  inv_proj_view   = inverse(projection * view);

    const auto ndc_x = (viewport_position.x / viewport_size.x) * 2.0f - 1.0f;
    const auto ndc_y = 1.0f - (viewport_position.y / viewport_size.y) * 2.0f;
    auto       far   = inv_proj_view * vec4f{ndc_x, ndc_y, 1.0f, 1.0f};
    if (std::abs(far.w) > 1e-6f) far /= far.w;

    const vec3f far_point = far.xyz;

    return {
        .origin    = global_eye,
        .direction = normalize(far_point - global_eye),
    };
}

namespace {
auto IntersectEditorTriangle(const EditorViewportRay& ray, vec3f a, vec3f b, vec3f c) noexcept -> std::optional<float> {
    constexpr auto epsilon = 1e-6f;

    const auto edge_ab = b - a;
    const auto edge_ac = c - a;
    const auto h       = cross(ray.direction, edge_ac);
    const auto det     = dot(edge_ab, h);
    if (std::abs(det) < epsilon) return std::nullopt;

    const auto inv_det = 1.0f / det;
    const auto s       = ray.origin - a;
    const auto u       = inv_det * dot(s, h);
    if (u < 0.0f || u > 1.0f) return std::nullopt;

    const auto q = cross(s, edge_ab);
    const auto v = inv_det * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) return std::nullopt;

    const auto t = inv_det * dot(edge_ac, q);
    if (t <= epsilon) return std::nullopt;
    return t;
}

template <asset::IndexType IndexType>
auto IntersectEditorMeshIndices(
    const EditorViewportRay&                         ray,
    const asset::Mesh&                               mesh,
    const mat4f&                                     transform,
    std::span<const asset::IndexDataType<IndexType>> indices,
    std::span<const vec3f>                           positions) noexcept -> std::optional<float> {
    std::optional<float> best;

    for (const auto& sub_mesh : mesh.sub_meshes) {
        const auto triangle_index_count = sub_mesh.index_count - sub_mesh.index_count % 3;
        for (std::size_t i = 0; i < triangle_index_count; i += 3) {
            const auto index_offset = sub_mesh.index_offset + i;
            if (index_offset + 2 >= indices.size()) break;

            const auto vertex_0 = static_cast<std::size_t>(indices[index_offset + 0]) + sub_mesh.vertex_offset;
            const auto vertex_1 = static_cast<std::size_t>(indices[index_offset + 1]) + sub_mesh.vertex_offset;
            const auto vertex_2 = static_cast<std::size_t>(indices[index_offset + 2]) + sub_mesh.vertex_offset;
            if (vertex_0 >= positions.size() || vertex_1 >= positions.size() || vertex_2 >= positions.size()) continue;

            const auto a = (transform * vec4f{positions[vertex_0], 1.0f}).xyz;
            const auto b = (transform * vec4f{positions[vertex_1], 1.0f}).xyz;
            const auto c = (transform * vec4f{positions[vertex_2], 1.0f}).xyz;

            const auto hit = IntersectEditorTriangle(ray, a, b, c);
            if (hit && (!best || *hit < *best)) best = hit;
        }
    }

    return best;
}

auto IntersectEditorMesh(const EditorViewportRay& ray, const asset::Mesh& mesh, const mat4f& transform) noexcept -> std::optional<float> {
    if (ray.direction.norm() <= 1e-6f || mesh.Empty()) return std::nullopt;

    const auto positions = mesh.vertices->Span<asset::VertexAttribute::Position>();
    if (positions.empty()) return std::nullopt;

    if (mesh.indices->Type() == asset::IndexType::UINT16) {
        return IntersectEditorMeshIndices<asset::IndexType::UINT16>(
            ray,
            mesh,
            transform,
            mesh.indices->Span<asset::IndexType::UINT16>(),
            positions);
    }

    return IntersectEditorMeshIndices<asset::IndexType::UINT32>(
        ray,
        mesh,
        transform,
        mesh.indices->Span<asset::IndexType::UINT32>(),
        positions);
}
}  // namespace

auto hitagi::PickEditorEntity(const asset::Scene& scene, const EditorViewportRay& ray, ecs::Entity ignored) -> std::optional<EditorPickResult> {
    std::optional<EditorPickResult> best;
    const auto                      root = scene.GetRootEntity();

    VisitEntityTree(root, [&](ecs::Entity entity) {
        if (entity == ignored) return;
        if (!IsEditorPickCandidate(entity, root)) return;

        const auto mesh = entity.Get<MeshComponent>().mesh;
        const auto hit  = IntersectEditorMesh(ray, *mesh, entity.Get<Transform>().world_matrix);
        if (!hit) return;

        if (!best || *hit < best->distance) {
            best = EditorPickResult{
                .entity   = entity,
                .distance = *hit,
            };
        }
    });

    return best;
}

SceneViewPort::SceneViewPort(const Engine& engine, EditorState& state, EditorCommandStack& command_stack)
    : core::RuntimeModule("SceneViewPort"),
      m_Engine(engine),
      m_State(state),
      m_CommandStack(command_stack),
      m_SelectionExtension(std::make_shared<EditorDeferredSelectionExtension>(
          engine.Device(),
          render::LoadShaderSource(engine.FileIO(), "hitagi/editor/shaders/editor_selection_outline.hlsl"))),
      m_GridPass(std::make_unique<EditorViewportGridPass>(
          engine.Device(),
          render::LoadShaderSource(engine.FileIO(), "hitagi/editor/shaders/viewport_grid.hlsl"))) {
    if (auto* deferred_renderer = dynamic_cast<render::DeferredRenderer*>(&m_Engine.Renderer())) {
        deferred_renderer->AddExtension(m_SelectionExtension);
    }
}

void hitagi::ApplyEditorViewportNavigation(
    const asset::Camera::Parameters&     camera_param,
    asset::Transform&                    camera_transform,
    EditorViewportNavigationState&       state,
    const EditorViewportNavigationInput& input) noexcept {
    const auto camera_basis = math::rotate(camera_transform.rotation);
    const auto world_look   = normalize(vec3f{(camera_basis * vec4f(camera_param.look_dir, 0.0f)).xyz});
    const auto world_up     = normalize(vec3f{(camera_basis * vec4f(camera_param.up, 0.0f)).xyz});
    const auto world_right  = normalize(math::cross(world_look, world_up));

    if (input.viewport_hovered && std::abs(input.scroll_delta.y) > 0.0f) {
        const auto rotated_eye = vec3f{(camera_basis * vec4f(camera_param.eye, 0.0f)).xyz};
        const auto eye_before  = camera_transform.position + rotated_eye;
        auto       offset      = eye_before - state.orbit_pivot;
        auto       distance    = offset.norm();
        if (distance <= 0.001f) {
            offset   = -world_look;
            distance = 1.0f;
        }

        const auto zoom_factor    = std::pow(0.82f, input.scroll_delta.y);
        const auto new_distance   = std::clamp(distance * zoom_factor, 0.01f, 100000.0f);
        camera_transform.position = state.orbit_pivot + normalize(offset) * new_distance - rotated_eye;
    }

    auto orbit_started = false;
    if (!input.middle_down) {
        state.orbiting = false;
    } else if (input.middle_down && !state.orbiting && input.viewport_hovered && state.has_orbit_pivot) {
        state.orbiting = true;
        orbit_started  = true;
    }

    if (!state.orbiting || orbit_started || input.mouse_delta.norm() <= 0.0f) return;

    const auto eye_before = camera_transform.position + vec3f{(math::rotate(camera_transform.rotation) * vec4f(camera_param.eye, 0.0f)).xyz};
    const auto distance   = std::max((eye_before - state.orbit_pivot).norm(), 0.001f);
    const auto yaw        = axis_angle_to_quaternion(world_up, -input.mouse_delta.x * 0.006f);
    const auto pitch_axis = normalize(vec3f{(math::rotate(yaw) * vec4f(world_right, 0.0f)).xyz});
    const auto pitch      = axis_angle_to_quaternion(pitch_axis, -input.mouse_delta.y * 0.006f);
    camera_transform.Rotate(pitch * yaw);

    const auto rotated_basis  = math::rotate(camera_transform.rotation);
    const auto rotated_eye    = vec3f{(rotated_basis * vec4f(camera_param.eye, 0.0f)).xyz};
    const auto rotated_look   = normalize(vec3f{(rotated_basis * vec4f(camera_param.look_dir, 0.0f)).xyz});
    camera_transform.position = state.orbit_pivot - rotated_look * distance - rotated_eye;
}

void SceneViewPort::Tick() {
    ZoneScopedN("SceneViewPort::Tick");

    m_Engine.GuiManager().DrawGuiEarly([&]() {
        ZoneScopedN("SceneViewPort UI");
        m_Open = m_State.IsPanelVisible(EditorPanel::SceneViewer);
        if (m_Open) {
            constexpr auto window_flags = ImGuiWindowFlags_NoCollapse;
            if (ImGui::Begin("Viewport", &m_Open, window_flags)) {
                {
                    ZoneScopedN("SceneViewPort Move Camera");
                    MoveCamera();
                }
                {
                    ZoneScopedN("SceneViewPort Render Scene");
                    RenderScene();
                }
            }
            ImGui::End();
            m_DefaultLayoutDone = true;
            m_State.SetPanelVisible(EditorPanel::SceneViewer, m_Open);
        }
    });

    core::RuntimeModule::Tick();
}

void SceneViewPort::MoveCamera() const {
    if (m_State.GetCurrentScene()) {
        auto& input_manager = m_Engine.App().GetInputManager();

        auto camera = m_Camera;

        const auto& camera_param     = camera.Get<asset::CameraComponent>().camera->parameters;
        auto&       camera_transform = camera.Get<asset::Transform>();

        ApplyEditorViewportNavigation(
            camera_param,
            camera_transform,
            m_CameraNavigation,
            EditorViewportNavigationInput{
                .delta_time       = static_cast<float>(m_Engine.GetDeltaTime().count()),
                .mouse_delta      = {input_manager.GetFloatDelta(hid::MouseEvent::MOVE_X), input_manager.GetFloatDelta(hid::MouseEvent::MOVE_Y)},
                .scroll_delta     = {input_manager.GetFloatDelta(hid::MouseEvent::SCROLL_X), input_manager.GetFloatDelta(hid::MouseEvent::SCROLL_Y)},
                .viewport_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows),
                .middle_down      = input_manager.GetBool(hid::VirtualKeyCode::MOUSE_M_BUTTON),
            });
    }
}

void SceneViewPort::RenderScene() const {
    ZoneScopedN("SceneViewPort::RenderScene");

    auto scene = m_State.GetCurrentScene();
    if (scene) {
        const auto v_min         = ImGui::GetWindowContentRegionMin();
        const auto v_max         = ImGui::GetWindowContentRegionMax();
        const auto viewport_size = ComputeViewportRenderSize(math::vec2u{
            static_cast<std::uint32_t>(v_max.x - v_min.x),
            static_cast<std::uint32_t>(v_max.y - v_min.y)});
        const auto logical_size  = viewport_size.logical;
        const auto physical_size = viewport_size.physical;

        auto& render_runtime = m_Engine.RenderRuntime();
        auto& render_graph   = render_runtime.GetRenderGraph();

        auto scene_render_texture = render_graph.Create(gfx::TextureDesc{
            .name        = "Scene Render Texture",
            .width       = physical_size.x,
            .height      = physical_size.y,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 1.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        });

        auto camera           = m_Camera.Get<asset::CameraComponent>().camera;
        auto camera_transform = m_Camera.Get<asset::Transform>().world_matrix;

        camera->parameters.aspect = static_cast<float>(physical_size.x) / static_cast<float>(physical_size.y);

        if (logical_size.x != 0 && logical_size.y != 0 && physical_size.x != 0 && physical_size.y != 0) {
            {
                ZoneScopedN("SceneViewPort Queue Scene Render");
                auto                context = render_runtime.MakeContext();
                EditorSelectionDesc selection_desc{
                    .enabled = m_State.GetMode() == EditorMode::Edit,
                };
                if (const auto selected = m_State.GetSelectedEntity()) {
                    selection_desc.items.emplace_back(EditorSelectionItem{
                        .entity       = selected,
                        .visual       = EditorSelectionVisual::Selected,
                        .selection_id = 1,
                    });
                }
                if (m_SelectionExtension) {
                    m_SelectionExtension->SetSelection(std::move(selection_desc));
                }
                std::pmr::vector<render::RenderDrawItem> draw_items;
                std::pmr::vector<render::RenderLight>    lights;
                CollectSceneRenderFrameData(*scene, draw_items, lights);
                scene_render_texture = m_Engine.Renderer().Render(
                                                              context,
                                                              render::RenderRequest{
                                                                  .frame = render::RenderFrame{
                                                                      .view       = BuildRenderView(*camera, camera_transform),
                                                                      .draw_items = draw_items,
                                                                      .lights     = lights,
                                                                  },
                                                                  .target            = scene_render_texture,
                                                                  .requested_outputs = {
                                                                      .depth        = true,
                                                                      .linear_depth = true,
                                                                      .normal       = true,
                                                                  },
                                                              })
                                           .color;
                scene_render_texture = m_GridPass->Build(context, *camera, camera_transform, scene_render_texture);
            }
            {
                ZoneScopedN("SceneViewPort Image");
                ImGui::Image(m_Engine.GuiManager().ReadTexture(scene_render_texture), ImVec2(logical_size.x, logical_size.y));
            }
            const auto image_min = ImGui::GetItemRectMin();
            {
                ZoneScopedN("SceneViewPort Overlays");
                DrawViewportOverlays({image_min.x, image_min.y}, {static_cast<float>(logical_size.x), static_cast<float>(logical_size.y)});
            }
            {
                ZoneScopedN("SceneViewPort Picking");
                HandlePicking({image_min.x, image_min.y}, {static_cast<float>(logical_size.x), static_cast<float>(logical_size.y)});
            }
            {
                ZoneScopedN("SceneViewPort Transform Gizmo");
                const_cast<SceneViewPort*>(this)->DrawTransformGizmo(
                    {image_min.x, image_min.y},
                    {static_cast<float>(logical_size.x), static_cast<float>(logical_size.y)});
            }
        }
    }
}

void SceneViewPort::HandlePicking(math::vec2f image_min, math::vec2f image_size) const {
    if (m_State.GetMode() != EditorMode::Edit || m_State.GetActiveTool() != EditorTool::Select) return;
    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left) || !m_Camera || !m_Camera.Has<CameraComponent>()) return;
    const auto& input_manager = m_Engine.App().GetInputManager();
    if (input_manager.GetBool(hid::VirtualKeyCode::KEY_ALT) ||
        input_manager.GetBool(hid::VirtualKeyCode::KEY_L_ALT) ||
        input_manager.GetBool(hid::VirtualKeyCode::KEY_R_ALT) ||
        input_manager.GetBool(hid::VirtualKeyCode::MOUSE_R_BUTTON) ||
        input_manager.GetBool(hid::VirtualKeyCode::MOUSE_M_BUTTON)) {
        return;
    }

    auto scene = m_State.GetCurrentScene();
    if (!scene) return;

    const auto mouse_position    = ImGui::GetMousePos();
    const auto viewport_position = vec2f{
        mouse_position.x - image_min.x,
        mouse_position.y - image_min.y,
    };

    auto camera = m_Camera.Get<CameraComponent>().camera;
    if (!camera) return;
    camera->parameters.aspect = image_size.x / image_size.y;

    const auto ray = BuildEditorViewportRay(*camera, m_Camera.Get<Transform>().world_matrix, viewport_position, image_size);
    if (auto pick = PickEditorEntity(*scene, ray, m_Camera)) {
        m_State.SelectEntity(pick->entity);
    } else {
        m_State.ClearSelection();
    }
}

void SceneViewPort::DrawTransformGizmo(math::vec2f image_min, math::vec2f image_size) {
    if (m_State.GetMode() != EditorMode::Edit) return;
    if (m_State.GetActiveTool() == EditorTool::Select) return;

    auto entity = m_State.GetSelectedEntity();
    if (!entity || !entity.Has<Transform>()) return;

    auto& transform = entity.Get<Transform>();
    auto  draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(
        ImVec2(image_min.x, image_min.y),
        ImVec2(image_min.x + image_size.x, image_min.y + image_size.y),
        IM_COL32(255, 201, 64, 180));

    ImGui::SetCursorScreenPos(ImVec2(image_min.x + 10.0f, image_min.y + 10.0f));
    ImGui::BeginGroup();
    ImGui::PushID("ViewportTransformGizmo");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.10f, 0.82f));
    ImGui::TextUnformatted(m_State.GetCoordinateSpace() == EditorCoordinateSpace::Local ? "Local" : "World");

    if (m_State.GetActiveTool() == EditorTool::Translate) {
        auto before = transform;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::DragFloat3("Move", transform.position, 0.1f) && m_State.IsSnapEnabled()) {
            transform.position = SnapVector(transform.position, m_State.GetTranslateSnap());
        }
        if (ImGui::IsItemActivated()) m_GizmoEditStart = before;
        CommitTransformEdit(entity, transform);
    } else if (m_State.GetActiveTool() == EditorTool::Rotate) {
        auto before = transform;
        auto euler  = quaternion_to_euler(transform.rotation);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::DragFloat3("Rotate", euler, 0.01f)) {
            if (m_State.IsSnapEnabled()) euler = SnapVector(euler, m_State.GetRotateSnap());
            transform.rotation = euler_to_quaternion(euler);
        }
        if (ImGui::IsItemActivated()) m_GizmoEditStart = before;
        CommitTransformEdit(entity, transform);
    } else if (m_State.GetActiveTool() == EditorTool::Scale) {
        auto before = transform;
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::DragFloat3("Scale", transform.scaling, 0.05f, 0.001f) && m_State.IsSnapEnabled()) {
            transform.scaling = SnapVector(transform.scaling, m_State.GetScaleSnap());
        }
        if (ImGui::IsItemActivated()) m_GizmoEditStart = before;
        CommitTransformEdit(entity, transform);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopID();
    ImGui::EndGroup();
}

void SceneViewPort::CommitTransformEdit(ecs::Entity entity, asset::Transform& transform) {
    if (ImGui::IsItemDeactivatedAfterEdit() && m_GizmoEditStart) {
        m_CommandStack.Execute(std::make_unique<TransformChangeCommand>(entity, *m_GizmoEditStart, transform));
        m_State.MarkDirty();
        m_GizmoEditStart.reset();
    }
}

void SceneViewPort::DrawViewportOverlays(math::vec2f image_min, math::vec2f image_size) const {
    auto draw_list = ImGui::GetWindowDrawList();

    auto scene = m_State.GetCurrentScene();
    if (!scene || !m_Camera || !m_Camera.Has<CameraComponent>() || !m_Camera.Has<Transform>()) {
        DrawViewportOrientationGizmo(draw_list, image_min, image_size);
        return;
    }

    const auto camera = m_Camera.Get<CameraComponent>().camera;
    if (!camera) {
        DrawViewportOrientationGizmo(draw_list, image_min, image_size);
        return;
    }

    const auto projection = BuildViewportProjection(*camera, m_Camera.Get<Transform>().world_matrix, image_min, image_size);
    const auto root       = scene->GetRootEntity();
    const auto selected   = m_State.GetSelectedEntity();

    if (selected && selected != m_Camera && selected.Has<Transform>()) {
        const auto bounds = EntityPickBounds(selected);
        DrawSelectedAxis(draw_list, projection, selected.Get<Transform>(), bounds);
        if (m_State.GetActiveTool() == EditorTool::Rotate) {
            DrawSelectedRotationOrbit(draw_list, projection, selected.Get<Transform>(), bounds);
        }
        draw_list->AddRect(
            ImVec2(image_min.x + 1.0f, image_min.y + 1.0f),
            ImVec2(image_min.x + image_size.x - 1.0f, image_min.y + image_size.y - 1.0f),
            IM_COL32(255, 201, 64, 160),
            0.0f,
            0,
            2.0f);
    }

    DrawViewportOrientationGizmo(draw_list, image_min, image_size);
}

void SceneViewPort::FocusSelectedEntity() {
    auto selected = m_State.GetSelectedEntity();
    if (!selected || !selected.Has<Transform>() || !m_Camera || !m_Camera.Has<CameraComponent>() || !m_Camera.Has<Transform>()) return;

    const auto camera = m_Camera.Get<CameraComponent>().camera;
    if (!camera) return;

    const auto bounds = EntityPickBounds(selected);
    if (!bounds.Valid()) return;

    const auto  target                 = bounds.Center();
    const auto  radius                 = std::max(bounds.Extents().norm(), 1.0f);
    const vec3f world_look_dir         = (m_Camera.Get<Transform>().world_matrix * vec4f(camera->parameters.look_dir, 0.0f)).xyz;
    const auto  look_dir               = normalize(world_look_dir);
    auto&       camera_xf              = m_Camera.Get<Transform>();
    const auto  rotated_eye            = vec3f{(math::rotate(camera_xf.rotation) * vec4f(camera->parameters.eye, 0.0f)).xyz};
    camera_xf.position                 = target - look_dir * (radius * 4.0f) - rotated_eye;
    m_CameraNavigation.orbit_pivot     = target;
    m_CameraNavigation.has_orbit_pivot = true;
    m_State.MarkDirty();
}

void SceneViewPort::SetScene(std::shared_ptr<asset::Scene> scene) noexcept {
    m_CameraNavigation.orbiting        = false;
    m_CameraNavigation.orbit_pivot     = {};
    m_CameraNavigation.has_orbit_pivot = true;

    if (scene != nullptr) {
        m_Camera = scene->GetCurrentCamera();
        if (!m_Camera) {
            m_Camera = scene->CreateCameraEntity(
                std::make_shared<asset::Camera>(CreateEditorDefaultCameraParameters(), "editor-camera"),
                math::mat4f::identity(),
                scene->GetRootEntity(),
                "view port camera");
        }
    } else {
        m_Camera = {};
    }
}
