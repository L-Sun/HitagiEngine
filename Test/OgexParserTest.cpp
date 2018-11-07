#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "OGEX.hpp"

using namespace My;
using namespace std;

namespace My {
MemoryManager* g_pMemoryManager = new MemoryManager();
AssetLoader*   g_pAssetLoader   = new AssetLoader();
}  // namespace My

template <typename Key, typename T>
static ostream& operator<<(ostream& out, unordered_map<Key, T> map) {
    for (auto p : map) {
        out << *p.second << endl;
    }
    return out;
}

int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();
    g_pAssetLoader->Initialize();

    string ogex_text =
        g_pAssetLoader->SyncOpenAndReadTextFileToString("Scene/cube.ogex");
    OgexParser*       ogex_parser = new OgexParser();
    unique_ptr<Scene> pScene      = ogex_parser->Parse(ogex_text);

    delete ogex_parser;
    cout << "Dump of Scene Graph" << endl;
    cout << "-------------------" << endl;
    cout << *pScene->SceneGraph << endl;

    cout << "Dump of Camera" << endl;
    cout << "-------------------" << endl;
    cout << pScene->Cameras << endl;

    cout << "Dump of Geometries" << endl;
    cout << "-------------------" << endl;
    cout << pScene->Geometries << endl;

    cout << "Dump of Light" << endl;
    cout << "-------------------" << endl;
    cout << pScene->Lights << endl;

    cout << "Dump of Material" << endl;
    cout << "-------------------" << endl;
    cout << pScene->Materials << endl;

    g_pMemoryManager->Finalize();
    g_pAssetLoader->Finalize();
    delete g_pMemoryManager;
    delete g_pAssetLoader;

    int x;
    cin >> x;
    return 0;
}
