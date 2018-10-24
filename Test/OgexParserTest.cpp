#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "SceneNode.hpp"
#include "OGEX.hpp"

using namespace My;
using namespace std;

namespace My {
MemoryManager* g_pMemoryManager = new MemoryManager();
}  // namespace My

int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();

    AssetLoader asset_loader;
    string      ogex_text =
        asset_loader.SyncOpenAndReadTextFileToString("Scene/Example.ogex");
    OgexParser*               ogex_parser = new OgexParser();
    unique_ptr<BaseSceneNode> root_node   = ogex_parser->Parse(ogex_text);
    delete ogex_parser;
    cout << *root_node << endl;
    g_pMemoryManager->Finalize();
    delete g_pMemoryManager;
    int x;
    cin >> x;
    return 0;
}
