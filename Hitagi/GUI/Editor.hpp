#pragma once
namespace Hitagi::Gui {
class Editor {
public:
    void MainMenu();
    void FileExplorer();

private:
    bool m_OpenFileExplorer = false;
};

}  // namespace Hitagi::Gui