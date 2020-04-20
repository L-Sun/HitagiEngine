#include "FileIOManager.hpp"
#include "TGA.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager> g_FileIOManager(new Core::FileIOManager);
}  // namespace Hitagi

int main(int argc, const char** argv) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

    Core::Buffer buf;
    if (argc >= 2) {
        buf = g_FileIOManager->SyncOpenAndReadBinary(argv[1]);
    } else {
        buf = g_FileIOManager->SyncOpenAndReadBinary("Asset/Textures/floor.tga");
    }

    Resource::TgaParser tga_arser;

    Resource::Image image = tga_arser.Parse(buf);

    std::cout << image;

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}