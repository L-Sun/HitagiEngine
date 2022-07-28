#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/resource/scene_object.hpp>

#include <string>

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

private:
    std::string m_OpenFileExt;
};

}  // namespace hitagi