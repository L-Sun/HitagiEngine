#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "OGEX.hpp"

using namespace My;

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_pAssetLoader(new AssetLoader);
}  // namespace My

template <typename Key, typename T>
std::ostream& operator<<(std::ostream& out, unordered_map<Key, T> map) {
    for (auto p : map) {
        out << *p.second << std::endl;
    }
    return out;
}

int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();
    g_pAssetLoader->Initialize();

    std::string ogex_text =
        g_pAssetLoader->SyncOpenAndReadTextFileToString("Scene/cube.ogex");
    OgexParser*            ogex_parser = new OgexParser();
    std::unique_ptr<Scene> pScene      = ogex_parser->Parse(ogex_text);

    delete ogex_parser;
    std::cout << "Dump of Scene Graph" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << *pScene->SceneGraph << std::endl;

    std::cout << "Dump of Camera" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << pScene->Cameras << std::endl;

    std::cout << "Dump of Geometries" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << pScene->Geometries << std::endl;

    std::cout << "Dump of Light" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << pScene->Lights << std::endl;

    std::cout << "Dump of Material" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << pScene->Materials << std::endl;

    g_pMemoryManager->Finalize();
    g_pAssetLoader->Finalize();

    return 0;
}
