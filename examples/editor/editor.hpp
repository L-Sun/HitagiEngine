#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <string>
#include <unordered_set>

namespace hitagi {
class Editor : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    void Draw();

    void MainMenu();
    void FileExplorer();
    void SceneExplorer();
    void DebugPanel();

private:
    struct DrawBone {
        static bool enable;
        static void OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta);
    };

    std::pmr::string                          m_OpenFileExt;
    std::pmr::unordered_set<std::pmr::string> m_SelectedFiles;
};

}  // namespace hitagi