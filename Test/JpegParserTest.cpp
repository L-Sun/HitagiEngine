#include <iostream>
#include <string>
#include "FileIOManager.hpp"
#include "MemoryManager.hpp"
#include "JPEG.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager> g_FileIOManager(new Core::FileIOManager);
}  // namespace Hitagi

int main(int argc, char const* argv[]) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

#ifdef __ORBIS__
    g_FileIOManager->AddSearchPath("/app0");
#endif

    {
        Core::Buffer buf;
        if (argc >= 2) {
            buf = g_FileIOManager->SyncOpenAndReadBinary(argv[1]);
        } else {
            buf = g_FileIOManager->SyncOpenAndReadBinary("Asset/Textures/b.jpg");
        }

        Resource::JpegParser jfif_parser;

        Resource::Image image = jfif_parser.Parse(buf);
        std::cout << image << std::endl;
    }

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}
