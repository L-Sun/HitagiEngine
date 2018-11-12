#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"

using namespace std;
using namespace My;

namespace My {
MemoryManager* g_pMemoryManager = new MemoryManager();
}  // namespace My

int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();

    AssetLoader asset_loader;
    string      shader_pgm =
        asset_loader.SyncOpenAndReadTextFileToString("Shaders/copy.vs");
    cout << shader_pgm << endl;
    g_pMemoryManager->Finalize();
    delete g_pMemoryManager;

    return 0;
}
