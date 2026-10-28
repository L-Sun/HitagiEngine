module;
#include "interop/tracy_macros.hpp"

module viewer;
import interop.tracy;

import std;
import asset;
import ecs;
import math;
import physics;
import editor;

namespace hitagi::examples {

void USDViewer::OpenScene(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::is_regular_file(path))
            throw std::invalid_argument(std::format("Scene does not exist: {}", path.string()));
        auto scene = m_Importer.Import(path);
        if (!scene) throw std::runtime_error(std::format("Cannot import scene: {}", path.string()));
        scene->GetWorld().Update(m_Jobs);
        if (scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().empty()) throw std::runtime_error("No mesh geometry in this file");
        ++m_SceneRevision;
        m_ViewStates.clear(); // Drop handles before destroying their owning scene.
        m_Scene = std::move(scene);
        m_Path  = path.lexically_normal();
        m_TreeRows.clear();
        m_AnimationPlayers.clear();
        m_Playing           = true;
        m_LastAnimationTick = std::chrono::steady_clock::now();
        m_Selected = {};
        SetGpuPalette(m_GpuPalette);
        RefreshSceneView();
        UpdateBounds();
        if (m_Bounds.Valid()) m_Orbit.Fit(m_Camera, m_Bounds);
        m_Error.clear();
        m_Status = "Scene loaded";
        m_App.SetWindowTitle(std::format("{} - Hitagi USD Viewer", m_Path.filename().string()));
    } catch (const std::exception& error) {
        m_Error  = error.what();
        m_Status = "Open failed; current scene preserved";
    }
}

void USDViewer::IndexScene(ecs::Entity entity, std::string parent, unsigned depth) {
    const auto path = parent + "/" + std::string(entity.Get<asset::MetaInfo>().name);
    m_TreeRows.push_back({entity, path, depth});
    for (const auto child : entity.Get<asset::RelationShip>().GetChildren())
        IndexScene(child, path, depth + 1);
}

void USDViewer::RefreshSceneView() {
    ZoneScopedNS("Viewer.RefreshSceneView", 0);
    // Detect structural edits even while paused. Names/relationships remain ECS-owned;
    // only rebuild display rows and inherited visibility when their inputs change.
    bool changed = std::erase_if(m_ViewStates, [](const auto& item) { return !item.first.Valid() || (item.second.observed && !item.first.template Has<asset::RelationShip>()); }) != 0;
    if (!m_Selected.Valid()) m_Selected = {};
    auto& entities = m_Scene->GetWorld().GetEntityManager();
    for (auto entity : entities.GetEntities<asset::RelationShip>()) {
        auto& state = m_ViewStates[entity];
        const auto& relation = entity.Get<asset::RelationShip>();
        const auto& name = entity.Get<asset::MetaInfo>().name;
        const auto children = relation.GetChildren();
        if (!state.observed || state.observed_parent != relation.parent ||
            std::string_view(state.observed_name) != std::string_view(name) ||
            !std::ranges::equal(state.observed_children, children)) {
            state.observed = true;
            state.observed_parent = relation.parent;
            state.observed_name = name;
            state.observed_children.assign(children.begin(), children.end());
            changed = true;
        }
    }
    if (changed) {
        m_TreeRows.clear();
        for (auto entity : entities.GetEntities<asset::RelationShip>())
            if (!entity.Get<asset::RelationShip>().parent) IndexScene(entity, "", 0);
        UpdateVisibility();
    }
    m_AnimationPlayers.clear();
    for (auto entity : entities.GetEntities<asset::AnimationPlayer>()) m_AnimationPlayers.push_back(entity);
}

void USDViewer::UpdateVisibility() {
    ZoneScopedNS("Viewer.UpdateVisibility", 0);
    const auto visit = [&](auto&& self, ecs::Entity entity, bool parent_visible) -> void {
        auto& state = m_ViewStates[entity];
        state.visible = parent_visible && !state.hidden;
        for (auto child : entity.Get<asset::RelationShip>().GetChildren()) self(self, child, state.visible);
    };
    for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<asset::RelationShip>())
        if (!entity.Get<asset::RelationShip>().parent) visit(visit, entity, true);
}

void USDViewer::UpdateBounds() {
    ZoneScopedNS("Viewer.UpdateBounds", 0);
    for (auto& [entity, state] : m_ViewStates) state.bounds = {};
    for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent, asset::Transform>()) {
        auto& node = m_ViewStates[entity];
        const auto& mesh = entity.Get<asset::MeshComponent>().mesh;
        if (mesh && !mesh->Empty()) {
            std::shared_ptr<const asset::VertexArray> vertices = mesh->vertices;
            if (entity.Has<asset::SkinnedMesh>()) {
                const auto& skin = entity.Get<asset::SkinnedMesh>();
                if (skin.pose) {
                    node.local_bounds = skin.pose->bounds;
                    node.bounds = math::transform_aabb(entity.Get<asset::Transform>().world_matrix, node.local_bounds);
                    continue;
                }
                const auto& deformed = skin.deformed_vertices;
                if (deformed) vertices = deformed;
            }
            if (!vertices) continue;
            // Geometry snapshots are immutable; only a new snapshot needs a scan.
            if (node.bounds_vertices != vertices) {
                node.local_bounds = {};
                for (const auto& position : vertices->Span<asset::VertexAttribute::Position>()) node.local_bounds.Expand(position);
                node.bounds_vertices = std::move(vertices);
            }
            node.bounds = math::transform_aabb(entity.Get<asset::Transform>().world_matrix, node.local_bounds);
        }
    }
    const auto aggregate = [&](auto&& self, ecs::Entity entity) -> math::AABBf {
        auto& bounds = m_ViewStates[entity].bounds;
        for (auto child : entity.Get<asset::RelationShip>().GetChildren()) {
            const auto child_bounds = self(self, child);
            if (child_bounds.Valid()) { bounds.Expand(child_bounds.min_point); bounds.Expand(child_bounds.max_point); }
        }
        return bounds;
    };
    m_Bounds = {};
    for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<asset::RelationShip>()) {
        if (entity.Get<asset::RelationShip>().parent) continue;
        const auto bounds = aggregate(aggregate, entity);
        if (bounds.Valid()) { m_Bounds.Expand(bounds.min_point); m_Bounds.Expand(bounds.max_point); }
    }
}

auto USDViewer::EntityKey(ecs::Entity entity) const -> std::string {
    return entity.Valid() ? std::format("{}:{}", m_SceneRevision, entity.GetId()) : "";
}

auto USDViewer::Visible(ecs::Entity entity) const -> bool {
    if (m_BypassVisibilityQuery) return true;
    const auto found = m_ViewStates.find(entity);
    return found == m_ViewStates.end() || found->second.visible;
}

void USDViewer::ApplyCommand(std::string_view command) {
    std::istringstream input{std::string(command)};
    std::string        action, value;
    input >> action >> std::quoted(value);
    if (action == "open")
        OpenScene(value);
    else if (action == "reload" && !m_Path.empty())
        OpenScene(m_Path);
    else if (action == "renderer")
        SetRenderer(value == "deferred" ? RendererType::Deferred :
                    value == "toon" ? RendererType::Toon : RendererType::Pbr);
    else if (action == "physics") {
        for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<physics::CharacterPhysics>()) {
            if (entity.Has<physics::CharacterPhysics>()) {
                auto& rig   = entity.Get<physics::CharacterPhysics>();
                rig.enabled = !rig.enabled;
                rig.reset = true;
            }
        }
        m_Scene->GetWorld().Update(m_Jobs);
        UpdateBounds();
    }
    else if (action == "animation" && !m_AnimationPlayers.empty()) {
        if (value == "toggle") m_Playing = !m_Playing;
        for (auto entity : m_AnimationPlayers) {
            auto& player   = entity.Get<asset::AnimationPlayer>();
            player.playing = m_Playing;
            if (value == "restart") {
                player.time = 0.0;
                if (entity.Has<physics::CharacterPhysics>()) entity.Get<physics::CharacterPhysics>().reset = true;
            }
        }
        m_LastAnimationTick = std::chrono::steady_clock::now();
        m_Scene->GetWorld().Update(m_Jobs);
        UpdateBounds();
    } else if (action == "select") {
        const auto row = std::ranges::find_if(m_TreeRows, [&](const auto& row) { return EntityKey(row.entity) == value; });
        if (row != m_TreeRows.end()) m_Selected = row->entity;
    } else if (action == "visibility") {
        if (m_Selected.Valid()) m_ViewStates[m_Selected].hidden = !m_ViewStates[m_Selected].hidden;
        UpdateVisibility();
    } else if (action == "show-all") {
        for (auto& [entity, state] : m_ViewStates) state.hidden = false;
        UpdateVisibility();
    } else if (action == "fit" && m_Bounds.Valid()) {
        const auto selected = m_ViewStates.find(m_Selected);
        const bool has_bounds = selected != m_ViewStates.end() && selected->second.bounds.Valid();
        m_Orbit.Fit(m_Camera, has_bounds ? selected->second.bounds : m_Bounds);
        m_Status = has_bounds ? "Framed selection" : "Framed scene";
    } else if (action == "home" && m_Bounds.Valid()) {
        m_Orbit.Fit(m_Camera, m_Bounds);
        m_Status = "Camera reset";
    } else if (action == "view")
        m_Orbit.SetView(m_Camera, value);
    else if (action == "light") {
        m_Light = std::min(std::max(m_Light + (value == "up" ? 0.25f : -0.25f), 0.25f), 4.0f);
    } else if (action == "debug" && m_Renderer == RendererType::Deferred) {
        using Debug = render::deferred::RenderGraphDebugView;
        if (value == "color")
            m_Debug = Debug::BaseColor;
        else if (value == "normal")
            m_Debug = Debug::Normal;
        else if (value == "roughness")
            m_Debug = Debug::Roughness;
        else
            m_Debug = Debug::Final;
    } else if (action == "dismiss")
        m_Error.clear();
}

}  // namespace hitagi::examples
