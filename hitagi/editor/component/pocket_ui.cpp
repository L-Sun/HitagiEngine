module editor;
import std;
import interop.nlohmann_json;

namespace hitagi {
auto FindUiEntity(const std::shared_ptr<asset::Scene>& scene, std::string_view id) -> ecs::Entity {
    ecs::Entity result;
    if (!scene) return result;
    const std::function<void(ecs::Entity)> visit = [&](ecs::Entity entity) {
        if (std::format("{}", entity) == id) result = entity;
        if (entity.Has<asset::RelationShip>())
            for (auto child : entity.Get<asset::RelationShip>().GetChildren()) visit(child);
    };
    visit(scene->GetRootEntity());
    return result;
}

void Editor::HandleUiCommands() {
    for (const auto& line : m_Engine.GuiManager().DrainEffects()) {
        try {
            const auto message = nlohmann::json::parse(line);
            if (message.value("type", std::string{}) != "editor.command") continue;
            const auto action   = message.at("action").get<std::string>();
            if (m_Game && action != "layout" && action != "play" && action != "pause" && action != "step" && action != "game.configure") continue;
            if (action == "game.configure" && m_Game) {
                if (m_State.GetMode() == EditorMode::Edit)
                    m_Game->Configure(message.at("field").get<std::string>(), message.at("delta").get<int>());
                continue;
            }
            auto       scene    = m_State.GetCurrentScene();
            auto       selected = m_State.GetSelectedEntity();
            if (action == "layout") {
                const auto rect = [&](std::string_view key) {
                    const auto values = message.at(std::string(key)).get<std::array<float, 4>>();
                    for (float value : values)
                        if (!std::isfinite(value) || value < 0 || value > 8192) throw std::invalid_argument("Invalid UI rectangle");
                    return math::vec4f{values[0], values[1], values[2], values[3]};
                };
                const auto scene_rect = rect("scene"), preview_rect = rect("preview");
                m_GameRect = scene_rect;
                if (m_SceneViewPort) m_SceneViewPort->SetRect(scene_rect);
                if (m_ImageViewer) m_ImageViewer->SetRect(preview_rect);
            } else if (action == "new")
                NewScene();
            else if (action == "undo")
                Undo();
            else if (action == "redo")
                Redo();
            else if (action == "play") {
                if (m_State.GetMode() == EditorMode::Edit)
                    EnterPlayMode();
                else
                    ExitPlayMode();
            } else if (action == "pause")
                TogglePauseMode();
            else if (action == "step")
                StepPlayMode();
            else if (action == "select")
                m_State.SelectEntity(FindUiEntity(scene, message.at("id").get<std::string>()));
            else if (action == "focus")
                m_SceneViewPort->FocusSelectedEntity();
            else if (action == "duplicate")
                DuplicateSelectedEntity();
            else if (action == "delete")
                DeleteSelectedEntity();
            else if (action == "create" && scene && m_State.GetMode() == EditorMode::Edit)
                ExecuteCommand(std::make_unique<CreateEmptyEntityCommand>(*scene, scene->GetRootEntity(), "Empty Entity", &m_State));
            else if (action == "rename" && scene && m_State.GetMode() == EditorMode::Edit) {
                const auto entity = FindUiEntity(scene, message.at("id").get<std::string>());
                const auto name = message.at("name").get<std::string>();
                if (entity && entity.Has<asset::MetaInfo>() && !name.empty() && name.size() <= 4096)
                    ExecuteCommand(std::make_unique<RenameEntityCommand>(*scene, entity, entity.Get<asset::MetaInfo>().name, name));
            }
            else if (action == "closePreview") {
                m_ImageViewer->SetTexture();
                m_AssetPreviewStatus.clear();
                m_AssetPreviewText.clear();
            } else if (action == "refresh")
                m_AssetBrowserModel.RequestRefresh();
            else if (action == "asset") {
                const auto  path    = message.at("path").get<std::string>();
                const auto& entries = m_AssetBrowserModel.GetEntries();
                const auto  it      = std::ranges::find_if(entries, [&](const auto& entry) { return std::string_view(entry.relative_path) == path; });
                if (it != entries.end()) {
                    if (it->kind == EditorAssetKind::Scene || it->kind == EditorAssetKind::Model)
                        ImportAssetFromBrowser(*it);
                    else
                        PreviewAssetFromBrowser(*it);
                }
            } else if (action == "transform" && selected && selected.Has<asset::Transform>() && m_State.GetMode() == EditorMode::Edit) {
                const auto axis  = message.at("axis").get<unsigned>();
                const auto delta = message.at("delta").get<float>();
                const auto field = message.at("field").get<std::string>();
                if (axis > 2 || !std::isfinite(delta) || std::abs(delta) > 1000) throw std::invalid_argument("Invalid transform delta");
                const auto before = selected.Get<asset::Transform>();
                auto       after  = before;
                if (field == "position")
                    after.position[axis] += delta;
                else if (field == "scale")
                    after.scaling[axis] = std::max(0.001f, after.scaling[axis] + delta);
                else if (field == "rotation") {
                    auto angles = math::quaternion_to_euler(after.rotation);
                    angles[axis] += delta;
                    after.rotation = math::euler_to_quaternion(angles);
                } else
                    throw std::invalid_argument("Unknown transform field");
                ExecuteCommand(std::make_unique<TransformChangeCommand>(selected, before, after));
            } else if (action == "component" && scene && selected && m_State.GetMode() == EditorMode::Edit) {
                const auto kind = message.at("kind").get<std::string>();
                if (kind != "camera" && kind != "light") throw std::invalid_argument("Unknown component");
                ExecuteCommand(std::make_unique<AddSceneComponentCommand>(*scene, selected,
                                                                          kind == "camera" ? SceneComponentKind::Camera : SceneComponentKind::Light, kind));
            }
        } catch (const std::exception& error) {
            Notify(std::format("UI command rejected: {}", error.what()));
        }
    }
}

void Editor::PublishUiState() {
    auto                                             nodes    = nlohmann::json::array();
    const auto                                       scene    = m_State.GetCurrentScene();
    const auto                                       selected = m_State.GetSelectedEntity();
    const std::function<void(ecs::Entity, unsigned)> visit    = [&](ecs::Entity entity, unsigned depth) {
        const auto id = std::format("{}", entity);
        nodes.push_back({{"id", id}, {"name", entity.Has<asset::MetaInfo>() ? std::string(entity.Get<asset::MetaInfo>().name) : id}, {"depth", depth}});
        if (entity.Has<asset::RelationShip>())
            for (auto child : entity.Get<asset::RelationShip>().GetChildren()) visit(child, depth + 1);
    };
    if (scene) visit(scene->GetRootEntity(), 0);
    auto assets = nlohmann::json::array();
    for (const auto& entry : m_AssetBrowserModel.GetEntries())
        assets.push_back({{"path", std::string(entry.relative_path)}, {"kind", std::string(EditorAssetKindName(entry.kind))}});
    nlohmann::json inspector = nullptr;
    if (selected && selected.Has<asset::Transform>()) {
        const auto& transform = selected.Get<asset::Transform>();
        const auto  rotation  = math::quaternion_to_euler(transform.rotation);
        inspector             = {{"position", {transform.position.x, transform.position.y, transform.position.z}},
                                 {"rotation", {rotation.x, rotation.y, rotation.z}},
                                 {"scale", {transform.scaling.x, transform.scaling.y, transform.scaling.z}}};
    }
    nlohmann::json value{
        {"kind", "editor"}, {"scene", scene ? std::string(scene->GetName()) : "No scene"}, {"mode", std::string(EditorModeName(m_State.GetMode()))}, {"dirty", m_State.IsDirty()}, {"canUndo", m_CommandStack.CanUndo()}, {"canRedo", m_CommandStack.CanRedo()}, {"nodes", std::move(nodes)}, {"selected", selected ? std::format("{}", selected) : ""}, {"transform", std::move(inspector)}, {"assets", std::move(assets)}, {"status", m_EditorNotifications.empty() ? "Ready" : std::string(m_EditorNotifications.back())}, {"assetError", std::string(m_AssetBrowserModel.GetLastError())}, {"preview", std::string(m_AssetPreviewStatus)}};
    if (m_Game) value["scene"] = "Game preview";
    auto state = nlohmann::json{{"type", "state"}, {"value", std::move(value)}}.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    if (state != m_LastUiState) {
        m_Engine.GuiManager().PostEvent(state);
        m_LastUiState = std::move(state);
    }
}
}  // namespace hitagi
