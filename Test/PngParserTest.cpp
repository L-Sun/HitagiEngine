#include <iostream>
#include <string>
#include "FileIOManager.hpp"
#include "MemoryManager.hpp"
#include "PNG.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager>        g_FileIOManager(new Core::FileIOManager);
}  // namespace Hitagi

int main(int argc, const char** argv) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

#ifdef __ORBIS__
    g_FileIOManager->AddSearchPath("/app0");
#endif

    Core::Buffer buf;
    if (argc >= 2) {
        buf = g_FileIOManager->SyncOpenAndReadBinary(argv[1]);
    } else {
        buf = g_FileIOManager->SyncOpenAndReadBinary("Textures/eye.png");
    }

    Resource::PngParser png_arser;

    Resource::Image image = png_arser.Parse(buf);

    std::cout << image;

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}