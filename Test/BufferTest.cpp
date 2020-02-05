#include "AssetLoader.hpp"
#include "OGEX.hpp"

#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_pAssetLoader(new AssetLoader);
}  // namespace My

using namespace My;
int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();
    {
        Buffer buf =
            g_pAssetLoader->SyncOpenAndReadBinary("Asset/Scene/balls.ogex");
        OgexParser parser;
        auto       pScene = parser.Parse(buf);
    }
    g_pMemoryManager->Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif
    return 0;
}
