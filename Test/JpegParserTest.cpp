#include <iostream>
#include <string>
#include "AssetLoader.hpp"
#include "MemoryManager.hpp"
#include "JPEG.hpp"

using namespace My;

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_pAssetLoader(new AssetLoader);
}  // namespace My

int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();
    g_pAssetLoader->Initialize();

#ifdef __ORBIS__
    g_pAssetLoader->AddSearchPath("/app0");
#endif

    {
        Buffer buf;
        if (argc >= 2) {
            buf = g_pAssetLoader->SyncOpenAndReadBinary(argv[1]);
        } else {
            buf = g_pAssetLoader->SyncOpenAndReadBinary("Textures/b.jpg");
        }

        JfifParser jfif_parser;

        Image image = jfif_parser.Parse(buf);
        std::cout << image;
    }

    g_pAssetLoader->Finalize();
    g_pMemoryManager->Finalize();

    return 0;
}
