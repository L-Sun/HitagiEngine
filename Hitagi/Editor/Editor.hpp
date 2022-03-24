#pragma once
#include "IRuntimeModule.hpp"
#include "SceneNode.hpp"
#include "Timer.hpp"

#include <string>

namespace hitagi {
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
    static std::string GenName(std::string_view name, std::shared_ptr<asset::SceneObject> obj);
    static std::string GenName(std::string_view name, std::shared_ptr<asset::SceneNode> node);

    std::string m_OpenFileExt;
};

}  // namespace hitagi