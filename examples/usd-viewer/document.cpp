module viewer;

import std;
import asset;
import ecs;
import math;
import physics;

namespace hitagi::examples {

void USDViewer::OpenScene(const std::filesystem::path& path) {
    try {
        auto scene = LoadScene(path, m_Jobs);
        if (scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().empty()) throw std::runtime_error("No mesh geometry in this file");
        m_Scene = std::move(scene);
        m_Path  = path.lexically_normal();
        m_Nodes.clear();
        m_AnimationPlayers.clear();
        m_Playing           = true;
        m_LastAnimationTick = std::chrono::steady_clock::now();
        m_Selected.clear();
        for (auto entity : m_Scene->GetWorld().GetEntityManager().GetEntities<asset::RelationShip>()) {
            if (!entity.Get<asset::RelationShip>().parent) IndexScene(entity, "", 0);
        }
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

void USDViewer::IndexScene(ecs::Entity entity, std::string parent, unsigned depth, std::size_t parent_index) {
    const auto name  = std::string(entity.Get<asset::MetaInfo>().name);
    const auto path  = parent + "/" + name;
    const auto index = m_Nodes.size();
    m_Nodes.push_back({.entity = entity, .path = path, .name = name, .depth = depth, .parent_index = parent_index});
    if (entity.Has<asset::AnimationPlayer>()) m_AnimationPlayers.emplace_back(entity);
    for (const auto child : entity.Get<asset::RelationShip>().GetChildren()) {
        IndexScene(child, path, depth + 1, index);
    }
}

void USDViewer::UpdateBounds() {
    for (auto& node : m_Nodes) {
        node.bounds = {};
        if (!node.entity.Has<asset::MeshComponent>()) continue;
        const auto& mesh = node.entity.Get<asset::MeshComponent>().mesh;
        if (mesh && !mesh->Empty()) {
            std::shared_ptr<const asset::VertexArray> vertices = mesh->vertices;
            if (node.entity.Has<asset::SkinnedMesh>()) {
                const auto& deformed = node.entity.Get<asset::SkinnedMesh>().deformed_vertices;
                if (deformed) vertices = deformed;
            }
            if (!vertices) continue;
            // Geometry snapshots are immutable; only a new snapshot needs a scan.
            if (node.bounds_vertices != vertices) {
                node.local_bounds = {};
                for (const auto& position : vertices->Span<asset::VertexAttribute::Position>()) node.local_bounds.Expand(position);
                node.bounds_vertices = std::move(vertices);
            }
            node.bounds = math::transform_aabb(node.entity.Get<asset::Transform>().world_matrix, node.local_bounds);
        }
    }
    for (std::size_t i = m_Nodes.size(); i-- > 0;) {
        const auto& bounds = m_Nodes[i].bounds;
        const auto  parent = m_Nodes[i].parent_index;
        if (parent == std::numeric_limits<std::size_t>::max()) continue;
        if (bounds.Valid()) {
            m_Nodes[parent].bounds.Expand(bounds.min_point);
            m_Nodes[parent].bounds.Expand(bounds.max_point);
        }
    }
    m_Bounds = {};
    for (const auto& node : m_Nodes) {
        if (node.parent_index != std::numeric_limits<std::size_t>::max() || !node.bounds.Valid()) continue;
        m_Bounds.Expand(node.bounds.min_point);
        m_Bounds.Expand(node.bounds.max_point);
    }
}

auto USDViewer::SelectedNode() -> SceneNode* {
    const auto node = std::ranges::find(m_Nodes, m_Selected, &SceneNode::path);
    return node == m_Nodes.end() ? nullptr : &*node;
}

auto USDViewer::Visible(const SceneNode& node) const -> bool {
    for (const auto& ancestor : m_Nodes) {
        if (ancestor.hidden && (node.path == ancestor.path || node.path.starts_with(ancestor.path + "/")))
            return false;
    }
    return true;
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
    else if (action == "physics-backend") {
        for (const auto& node : m_Nodes) {
            auto entity = node.entity;
            if (!entity.Has<physics::CharacterPhysics>()) continue;
            auto& rig   = entity.Get<physics::CharacterPhysics>();
            rig.backend = rig.backend == physics::RigBackend::Bullet ? physics::RigBackend::Jolt : physics::RigBackend::Bullet;
            rig.reset = true;
        }
        m_Scene->GetWorld().Update(m_Jobs);
        UpdateBounds();
    }
    else if (action == "physics") {
        for (const auto& node : m_Nodes) {
            auto entity = node.entity;
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
        if (std::ranges::find(m_Nodes, value, &SceneNode::path) != m_Nodes.end()) m_Selected = value;
    } else if (action == "visibility") {
        if (auto* node = SelectedNode()) node->hidden = !node->hidden;
    } else if (action == "show-all") {
        for (auto& node : m_Nodes) node.hidden = false;
    } else if (action == "fit" && m_Bounds.Valid()) {
        auto* selected = SelectedNode();
        m_Orbit.Fit(m_Camera, selected && selected->bounds.Valid() ? selected->bounds : m_Bounds);
        m_Status = selected ? "Framed selection" : "Framed scene";
    } else if (action == "home" && m_Bounds.Valid()) {
        m_Orbit.Fit(m_Camera, m_Bounds);
        m_Status = "Camera reset";
    } else if (action == "view")
        m_Orbit.SetView(m_Camera, value);
    else if (action == "light") {
        m_Light = std::min(std::max(m_Light + (value == "up" ? 0.25f : -0.25f), 0.25f), 4.0f);
    } else if (action == "debug" && m_Deferred) {
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
