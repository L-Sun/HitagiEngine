module viewer;

import std;
import asset;
import math;
import physics;

namespace hitagi::examples {

auto JsonString(std::string_view value) -> std::string {
    std::string result = "\"";
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            result += '\\';
            result += c;
        } else if (c < 32)
            result += std::format("\\u{:04x}", c);
        else
            result += c;
    }
    return result + '"';
}

auto USDViewer::Snapshot() const -> std::string {
    std::string nodes  = "[";
    unsigned    meshes = 0, visible = 0, triangles = 0;
    std::string inspector = "null";
    for (const auto& node : m_Nodes) {
        const bool  mesh_node = node.entity.Has<asset::MeshComponent>();
        unsigned    faces     = 0;
        std::string materials;
        if (mesh_node) {
            ++meshes;
            if (Visible(node)) ++visible;
            const auto& mesh = node.entity.Get<asset::MeshComponent>().mesh;
            for (const auto& part : mesh->sub_meshes) {
                faces += static_cast<unsigned>(part.index_count / 3);
                if (part.material) {
                    if (!materials.empty()) materials += ", ";
                    materials += part.material->GetName();
                }
            }
            triangles += faces;
        }
        if (nodes.size() > 1) nodes += ',';
        nodes +=
            std::format(R"({{"id":{},"name":{},"depth":{},"mesh":{},"visible":{}}})", JsonString(node.path),
                        JsonString(node.name), node.depth, mesh_node, Visible(node));
        if (node.path == m_Selected) {
            const auto& transform = node.entity.Get<asset::Transform>();
            inspector             = std::format(
                R"({{"name":{},"path":{},"type":"{}","visible":{},"triangles":{},"materials":{},"position":[{},{},{}],"scale":[{},{},{}]}})",
                JsonString(node.name), JsonString(node.path), mesh_node ? "Mesh" : "Transform", Visible(node),
                faces, JsonString(materials), transform.position.x, transform.position.y,
                transform.position.z, transform.scaling.x, transform.scaling.y, transform.scaling.z);
        }
    }
    nodes += ']';
    double animation_time = 0.0, animation_duration = 0.0;
    unsigned physics_bodies = 0;
    bool physics = false;
    std::string solver = "";
    for (const auto& node : m_Nodes) {
        if (node.entity.Has<physics::CharacterPhysics>()) {
            const auto& rig = node.entity.Get<physics::CharacterPhysics>();
            physics_bodies += static_cast<unsigned>(rig.bodies.size());
            solver = rig.backend == hitagi::physics::RigBackend::Jolt ? "Jolt" : "Bullet";
            physics = physics || rig.enabled;
        }
    }
    if (!m_AnimationPlayers.empty()) {
        const auto& player = m_AnimationPlayers.front().Get<asset::AnimationPlayer>();
        animation_time     = player.time;
        animation_duration = player.clip ? player.clip->GetDuration() : 0.0;
    }
    const auto& eye = m_Orbit.Eye();
    return std::format(
        R"({{"kind":"viewer","path":{},"file":{},"renderer":"{}","backend":{},"debug":{},"light":{},"selected":{},"nodes":{},"inspector":{},"meshes":{},"visible":{},"triangles":{},"error":{},"status":{},"eye":[{},{},{}],"animations":{},"playing":{},"animationTime":{},"animationDuration":{},"physicsBodies":{},"physics":{},"physicsBackend":{}}})",
        JsonString(m_Path.generic_string()), JsonString(m_Path.filename().string()),
        RendererName(m_Renderer), JsonString(m_App.GetConfig().gfx_backend),
        static_cast<unsigned>(m_Debug), m_Light, JsonString(m_Selected), nodes, inspector, meshes, visible,
        triangles, JsonString(m_Error), JsonString(m_Status), eye.x, eye.y, eye.z,
        m_AnimationPlayers.size(), m_Playing, animation_time, animation_duration, physics_bodies, physics, JsonString(solver));
}

void USDViewer::PublishState() {
    auto state = Snapshot();
    if (state == m_PublishedState) return;
    // Timeline text does not need to reconcile the entire scene tree at the
    // render rate. All other state changes (controls, camera, selection, errors)
    // still publish immediately; Snapshot() remains unthrottled for replay.
    auto static_state = state;
    const auto time_key = static_state.find("\"animationTime\":");
    if (time_key != std::string::npos) {
        const auto value = time_key + std::string_view("\"animationTime\":").size();
        static_state.erase(value, static_state.find(',', value) - value);
    }
    const auto now = std::chrono::steady_clock::now();
    const bool timeline_due = m_Deterministic ? m_Frame % 6 == 0 :
        now - m_LastStatePublish >= std::chrono::milliseconds(100);
    if (static_state == m_PublishedStaticState && !timeline_due) return;
    m_Gui.PostEvent("{\"type\":\"state\",\"value\":" + state + "}");
    m_PublishedState = std::move(state);
    m_PublishedStaticState = std::move(static_state);
    m_LastStatePublish = now;
}

void USDViewer::DrainUi() {
    for (const auto& effect : m_Gui.DrainEffects()) {
        if (!effect.starts_with("viewer ")) continue;
        const auto         command = std::string_view(effect).substr(7);
        std::istringstream input{std::string(command)};
        std::string        action;
        input >> action;
        if (action == "layout") {
            math::vec4f rect;
            if (!(input >> rect.x >> rect.y >> rect.z >> rect.w))
                throw std::runtime_error("Invalid viewer layout effect");
            m_Viewport = rect;
        } else if (action == "control") {
            std::string id;
            UiControl   control;
            if (!(input >> std::quoted(id) >> control.rect.x >> control.rect.y >> control.rect.z >>
                  control.rect.w >> control.enabled))
                throw std::runtime_error("Invalid viewer control effect");
            m_Controls[id] = control;
        } else if (action == "remove") {
            std::string id;
            input >> std::quoted(id);
            m_Controls.erase(id);
        } else
            ApplyCommand(command);
    }
}

}  // namespace hitagi::examples
