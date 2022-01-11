#pragma once
#include "IRuntimeModule.hpp"

#include <string>

namespace Hitagi {
class Editor : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void Draw();

    void MainMenu();
    void FileExplorer();
    void SceneExplorer();

private:
    std::string m_OpenFileExt;
    bool        m_OpenFileExplorer = false;
};

}  // namespace Hitagi