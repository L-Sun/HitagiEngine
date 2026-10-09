export module editor:command;
import engine;
import :state;

export namespace hitagi {
class EditorCommand {
public:
    virtual ~EditorCommand() = default;

    virtual void Execute() = 0;
    virtual void Undo()    = 0;
    virtual auto GetLabel() const noexcept -> std::string_view = 0;
};

class EditorCommandStack {
public:
    void Execute(std::unique_ptr<EditorCommand> command) {
        if (!command) return;

        command->Execute();
        if (m_NextCommand < m_Commands.size()) {
            m_Commands.erase(m_Commands.begin() + static_cast<std::ptrdiff_t>(m_NextCommand), m_Commands.end());
        }
        m_Commands.emplace_back(std::move(command));
        ++m_NextCommand;
        ++m_Revision;
    }

    void Undo() {
        if (!CanUndo()) return;

        --m_NextCommand;
        m_Commands[m_NextCommand]->Undo();
        ++m_Revision;
    }

    void Redo() {
        if (!CanRedo()) return;

        m_Commands[m_NextCommand]->Execute();
        ++m_NextCommand;
        ++m_Revision;
    }

    void Clear() {
        m_Commands.clear();
        m_NextCommand       = 0;
        m_CleanCommandIndex = 0;
        m_Revision          = 0;
    }

    void MarkClean() noexcept { m_CleanCommandIndex = m_NextCommand; }

    auto CanUndo() const noexcept -> bool { return m_NextCommand > 0; }
    auto CanRedo() const noexcept -> bool { return m_NextCommand < m_Commands.size(); }
    auto IsDirty() const noexcept -> bool { return m_NextCommand != m_CleanCommandIndex; }
    auto GetRevision() const noexcept -> std::uint64_t { return m_Revision; }

    auto GetUndoLabel() const noexcept -> std::string_view {
        return CanUndo() ? m_Commands[m_NextCommand - 1]->GetLabel() : std::string_view{};
    }

    auto GetRedoLabel() const noexcept -> std::string_view {
        return CanRedo() ? m_Commands[m_NextCommand]->GetLabel() : std::string_view{};
    }

private:
    std::pmr::vector<std::unique_ptr<EditorCommand>> m_Commands;
    std::size_t                                      m_NextCommand       = 0;
    std::size_t                                      m_CleanCommandIndex = 0;
    std::uint64_t                                    m_Revision          = 0;
};

class TransformChangeCommand final : public EditorCommand {
public:
    TransformChangeCommand(ecs::Entity entity, asset::Transform before, asset::Transform after)
        : m_Entity(entity), m_Before(before), m_After(after) {}

    void Execute() final { Apply(m_After); }
    void Undo() final { Apply(m_Before); }
    auto GetLabel() const noexcept -> std::string_view final { return "Change Transform"; }

private:
    void Apply(const asset::Transform& transform) {
        if (!m_Entity || !m_Entity.Has<asset::Transform>()) return;
        auto& target    = m_Entity.Get<asset::Transform>();
        target.position = transform.position;
        target.rotation = transform.rotation;
        target.scaling  = transform.scaling;
    }

    ecs::Entity      m_Entity;
    asset::Transform m_Before;
    asset::Transform m_After;
};

class MaterialParameterChangeCommand final : public EditorCommand {
public:
    MaterialParameterChangeCommand(
        std::shared_ptr<asset::Material> material,
        asset::MaterialParameter         before,
        asset::MaterialParameter         after)
        : m_Material(std::move(material)), m_Before(std::move(before)), m_After(std::move(after)) {}

    void Execute() final { Apply(m_After); }
    void Undo() final { Apply(m_Before); }
    auto GetLabel() const noexcept -> std::string_view final { return "Change Material Parameter"; }

private:
    void Apply(const asset::MaterialParameter& parameter) {
        if (!m_Material) return;
        std::visit(
            [&](const auto& value) {
                m_Material->SetParameter(parameter.name, value);
            },
            parameter.value);
    }

    std::shared_ptr<asset::Material> m_Material;
    asset::MaterialParameter         m_Before;
    asset::MaterialParameter         m_After;
};

class CameraParameterChangeCommand final : public EditorCommand {
public:
    CameraParameterChangeCommand(
        std::shared_ptr<asset::Camera> camera,
        asset::Camera::Parameters      before,
        asset::Camera::Parameters      after)
        : m_Camera(std::move(camera)), m_Before(before), m_After(after) {}

    void Execute() final { Apply(m_After); }
    void Undo() final { Apply(m_Before); }
    auto GetLabel() const noexcept -> std::string_view final { return "Change Camera"; }

private:
    void Apply(const asset::Camera::Parameters& parameters) {
        if (m_Camera) m_Camera->parameters = parameters;
    }

    std::shared_ptr<asset::Camera> m_Camera;
    asset::Camera::Parameters      m_Before;
    asset::Camera::Parameters      m_After;
};

class LightParameterChangeCommand final : public EditorCommand {
public:
    LightParameterChangeCommand(
        std::shared_ptr<asset::Light> light,
        asset::Light::Parameters      before,
        asset::Light::Parameters      after)
        : m_Light(std::move(light)), m_Before(before), m_After(after) {}

    void Execute() final { Apply(m_After); }
    void Undo() final { Apply(m_Before); }
    auto GetLabel() const noexcept -> std::string_view final { return "Change Light"; }

private:
    void Apply(const asset::Light::Parameters& parameters) {
        if (m_Light) m_Light->parameters = parameters;
    }

    std::shared_ptr<asset::Light> m_Light;
    asset::Light::Parameters      m_Before;
    asset::Light::Parameters      m_After;
};

enum struct SceneComponentKind : std::uint8_t {
    Camera,
    Light,
};

class AddSceneComponentCommand final : public EditorCommand {
public:
    AddSceneComponentCommand(asset::Scene& scene, ecs::Entity entity, SceneComponentKind kind, std::string_view name)
        : m_Scene(scene), m_Entity(entity), m_Kind(kind), m_Name(name) {}

    void Execute() final {
        m_Applied = false;
        if (m_Kind == SceneComponentKind::Camera) {
            if (!m_Camera) m_Camera = std::make_shared<asset::Camera>(asset::Camera::Parameters{}, m_Name);
            m_Applied = m_Scene.AddCameraComponent(m_Entity, m_Camera);
        } else {
            if (!m_Light) m_Light = std::make_shared<asset::Light>(asset::Light::Parameters{}, m_Name);
            m_Applied = m_Scene.AddLightComponent(m_Entity, m_Light);
        }
        m_Scene.Update();
    }

    void Undo() final {
        if (!m_Applied) return;
        if (m_Kind == SceneComponentKind::Camera) {
            m_Scene.RemoveCameraComponent(m_Entity);
        } else {
            m_Scene.RemoveLightComponent(m_Entity);
        }
        m_Scene.Update();
    }

    auto GetLabel() const noexcept -> std::string_view final {
        return m_Kind == SceneComponentKind::Camera ? "Add Camera Component" : "Add Light Component";
    }

private:
    asset::Scene&                  m_Scene;
    ecs::Entity                    m_Entity;
    SceneComponentKind             m_Kind;
    std::pmr::string               m_Name;
    std::shared_ptr<asset::Camera> m_Camera;
    std::shared_ptr<asset::Light>  m_Light;
    bool                           m_Applied = false;
};

class RemoveSceneComponentCommand final : public EditorCommand {
public:
    RemoveSceneComponentCommand(asset::Scene& scene, ecs::Entity entity, SceneComponentKind kind)
        : m_Scene(scene), m_Entity(entity), m_Kind(kind) {}

    void Execute() final {
        m_Applied = false;
        if (m_Kind == SceneComponentKind::Camera) {
            m_Camera  = m_Scene.RemoveCameraComponent(m_Entity);
            m_Applied = m_Camera != nullptr;
        } else {
            m_Light   = m_Scene.RemoveLightComponent(m_Entity);
            m_Applied = m_Light != nullptr;
        }
        m_Scene.Update();
    }

    void Undo() final {
        if (!m_Applied) return;
        if (m_Kind == SceneComponentKind::Camera) {
            m_Scene.AddCameraComponent(m_Entity, m_Camera);
        } else {
            m_Scene.AddLightComponent(m_Entity, m_Light);
        }
        m_Scene.Update();
    }

    auto GetLabel() const noexcept -> std::string_view final {
        return m_Kind == SceneComponentKind::Camera ? "Remove Camera Component" : "Remove Light Component";
    }

private:
    asset::Scene&                  m_Scene;
    ecs::Entity                    m_Entity;
    SceneComponentKind             m_Kind;
    std::shared_ptr<asset::Camera> m_Camera;
    std::shared_ptr<asset::Light>  m_Light;
    bool                           m_Applied = false;
};

class CreateEmptyEntityCommand final : public EditorCommand {
public:
    CreateEmptyEntityCommand(asset::Scene& scene, ecs::Entity parent, std::string_view name, EditorState* state = nullptr)
        : m_Scene(scene), m_Parent(parent), m_Name(name), m_State(state) {}

    void Execute() final {
        m_Entity = m_Scene.CreateEmptyEntity(math::mat4f::identity(), m_Parent, m_Name);
        m_Scene.Update();
        if (m_State) m_State->SelectEntity(m_Entity);
    }

    void Undo() final {
        if (m_State && m_State->GetSelectedEntity() == m_Entity) m_State->ClearSelection();
        m_Scene.DestroyEntitySubtree(m_Entity);
        m_Scene.Update();
        m_Entity = {};
    }

    auto GetLabel() const noexcept -> std::string_view final { return "Create Entity"; }
    auto GetEntity() const noexcept -> ecs::Entity { return m_Entity; }

private:
    asset::Scene&     m_Scene;
    ecs::Entity       m_Parent;
    std::pmr::string  m_Name;
    EditorState*      m_State = nullptr;
    ecs::Entity       m_Entity;
};

class RenameEntityCommand final : public EditorCommand {
public:
    RenameEntityCommand(asset::Scene& scene, ecs::Entity entity, std::string_view before, std::string_view after)
        : m_Scene(scene), m_Entity(entity), m_Before(before), m_After(after) {}

    void Execute() final { m_Scene.RenameEntity(m_Entity, m_After); }
    void Undo() final { m_Scene.RenameEntity(m_Entity, m_Before); }
    auto GetLabel() const noexcept -> std::string_view final { return "Rename Entity"; }

private:
    asset::Scene&    m_Scene;
    ecs::Entity      m_Entity;
    std::pmr::string m_Before;
    std::pmr::string m_After;
};

class ReparentEntityCommand final : public EditorCommand {
public:
    ReparentEntityCommand(asset::Scene& scene, ecs::Entity entity, ecs::Entity before_parent, ecs::Entity after_parent)
        : m_Scene(scene), m_Entity(entity), m_BeforeParent(before_parent), m_AfterParent(after_parent) {}

    void Execute() final { m_Scene.ReparentEntity(m_Entity, m_AfterParent); }
    void Undo() final { m_Scene.ReparentEntity(m_Entity, m_BeforeParent); }
    auto GetLabel() const noexcept -> std::string_view final { return "Reparent Entity"; }

private:
    asset::Scene& m_Scene;
    ecs::Entity   m_Entity;
    ecs::Entity   m_BeforeParent;
    ecs::Entity   m_AfterParent;
};

struct EntitySubtreeSnapshot {
    std::pmr::string name;
    asset::Transform transform;
    bool has_transform = false;

    std::shared_ptr<asset::Mesh> mesh;
    std::shared_ptr<asset::Camera> camera;
    std::shared_ptr<asset::Light> light;

    std::pmr::vector<EntitySubtreeSnapshot> children;
};

inline auto CaptureEntitySubtree(ecs::Entity entity) -> EntitySubtreeSnapshot {
    EntitySubtreeSnapshot snapshot;
    if (entity.Has<asset::MetaInfo>()) {
        snapshot.name = entity.Get<asset::MetaInfo>().name;
    } else {
        snapshot.name = std::pmr::string{std::format("Entity {}", entity.GetId())};
    }

    if (entity.Has<asset::Transform>()) {
        snapshot.transform = entity.Get<asset::Transform>();
        snapshot.has_transform = true;
    }
    if (entity.Has<asset::MeshComponent>()) {
        snapshot.mesh = entity.Get<asset::MeshComponent>().mesh;
    }
    if (entity.Has<asset::CameraComponent>()) {
        snapshot.camera = entity.Get<asset::CameraComponent>().camera;
    }
    if (entity.Has<asset::LightComponent>()) {
        snapshot.light = entity.Get<asset::LightComponent>().light;
    }
    if (entity.Has<asset::RelationShip>()) {
        for (const auto child : entity.Get<asset::RelationShip>().GetChildren()) {
            snapshot.children.emplace_back(CaptureEntitySubtree(child));
        }
    }
    return snapshot;
}

inline auto RestoreEntitySubtree(asset::Scene& scene, const EntitySubtreeSnapshot& snapshot, ecs::Entity parent) -> ecs::Entity {
    const auto transform = snapshot.has_transform ? snapshot.transform.ToMatrix() : math::mat4f::identity();

    ecs::Entity entity;
    if (snapshot.mesh) {
        entity = scene.CreateMeshEntity(snapshot.mesh, transform, parent, snapshot.name);
    } else if (snapshot.camera) {
        entity = scene.CreateCameraEntity(snapshot.camera, transform, parent, snapshot.name);
    } else if (snapshot.light) {
        entity = scene.CreateLightEntity(snapshot.light, transform, parent, snapshot.name);
    } else {
        entity = scene.CreateEmptyEntity(transform, parent, snapshot.name);
    }

    for (const auto& child : snapshot.children) {
        RestoreEntitySubtree(scene, child, entity);
    }
    return entity;
}

class DeleteEntitySubtreeCommand final : public EditorCommand {
public:
    DeleteEntitySubtreeCommand(asset::Scene& scene, ecs::Entity entity, EditorState* state = nullptr)
        : m_Scene(scene), m_Parent(entity && entity.Has<asset::RelationShip>() ? entity.Get<asset::RelationShip>().parent : ecs::Entity{}), m_State(state) {
        if (entity) {
            m_Snapshot = CaptureEntitySubtree(entity);
            m_Entity = entity;
        }
    }

    void Execute() final {
        if (!m_Entity) return;
        if (m_State && m_State->GetSelectedEntity() == m_Entity) m_State->ClearSelection();
        m_Scene.DestroyEntitySubtree(m_Entity);
        m_Scene.Update();
        m_Entity = {};
    }

    void Undo() final {
        m_Entity = RestoreEntitySubtree(m_Scene, m_Snapshot, m_Parent);
        m_Scene.Update();
        if (m_State) m_State->SelectEntity(m_Entity);
    }

    auto GetLabel() const noexcept -> std::string_view final { return "Delete Entity"; }

private:
    asset::Scene&          m_Scene;
    ecs::Entity            m_Parent;
    EntitySubtreeSnapshot  m_Snapshot;
    EditorState*           m_State = nullptr;
    ecs::Entity            m_Entity;
};

class DuplicateEntitySubtreeCommand final : public EditorCommand {
public:
    DuplicateEntitySubtreeCommand(asset::Scene& scene, ecs::Entity entity, EditorState* state = nullptr)
        : m_Scene(scene), m_Parent(entity && entity.Has<asset::RelationShip>() ? entity.Get<asset::RelationShip>().parent : ecs::Entity{}), m_State(state) {
        if (entity) {
            m_Snapshot = CaptureEntitySubtree(entity);
            m_Snapshot.name += " Copy";
        }
    }

    void Execute() final {
        m_Entity = RestoreEntitySubtree(m_Scene, m_Snapshot, m_Parent);
        m_Scene.Update();
        if (m_State) m_State->SelectEntity(m_Entity);
    }

    void Undo() final {
        if (m_State && m_State->GetSelectedEntity() == m_Entity) m_State->ClearSelection();
        m_Scene.DestroyEntitySubtree(m_Entity);
        m_Scene.Update();
        m_Entity = {};
    }

    auto GetLabel() const noexcept -> std::string_view final { return "Duplicate Entity"; }

private:
    asset::Scene&          m_Scene;
    ecs::Entity            m_Parent;
    EntitySubtreeSnapshot  m_Snapshot;
    EditorState*           m_State = nullptr;
    ecs::Entity            m_Entity;
};
}  // namespace hitagi
