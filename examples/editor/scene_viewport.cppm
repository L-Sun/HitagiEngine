module;

export module editor:scene_viewport;
import engine;

export namespace hitagi {
class SceneViewPort : public core::RuntimeModule {
public:
    SceneViewPort(const Engine& engine)
        : core::RuntimeModule("SceneViewPort"), m_Engine(engine) {}

    void Tick() final;

    void SetScene(std::shared_ptr<asset::Scene> scene) noexcept;

    inline auto GetScene() const noexcept { return m_CurrentScene; };

private:
    void MoveCamera() const;
    void RenderScene() const;

    const Engine&                 m_Engine;
    bool                          m_Open         = true;
    std::shared_ptr<asset::Scene> m_CurrentScene = nullptr;
    ecs::Entity                   m_Camera;
};
}  // namespace hitagi
