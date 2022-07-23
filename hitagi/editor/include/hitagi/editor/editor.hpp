#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/scene_node.hpp>

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
    static std::string GenName(std::string_view name, std::shared_ptr<resource::ResourceObject> obj);
    static std::string GenName(std::string_view name, std::shared_ptr<resource::SceneNode> node);

    std::string m_OpenFileExt;
};

}  // namespace hitagi