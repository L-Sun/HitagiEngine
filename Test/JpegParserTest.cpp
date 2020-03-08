#include <iostream>
#include <string>
#include "AssetLoader.hpp"
#include "MemoryManager.hpp"
#include "JPEG.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<MemoryManager> g_MemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_AssetLoader(new AssetLoader);
}  // namespace Hitagi

int main(int argc, char const* argv[]) {
    g_MemoryManager->Initialize();
    g_AssetLoader->Initialize();

#ifdef __ORBIS__
    g_AssetLoader->AddSearchPath("/app0");
#endif

    {
        Buffer buf;
        if (argc >= 2) {
            buf = g_AssetLoader->SyncOpenAndReadBinary(argv[1]);
        } else {
            buf = g_AssetLoader->SyncOpenAndReadBinary("Asset/Textures/b.jpg");
        }

        JpegParser jfif_parser;

        Image image = jfif_parser.Parse(buf);
        std::cout << image;
    }

    g_AssetLoader->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}
