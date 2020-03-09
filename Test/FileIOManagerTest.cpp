#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "FileIOManager.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager(new Core::MemoryManager);
}  // namespace Hitagi

int main(int argc, char const* argv[]) {
    g_MemoryManager->Initialize();

    Core::FileIOManager asset_loader;
    std::string  shader_pgm = asset_loader.SyncOpenAndReadTextFileToString("Asset/Shaders/basic.vs");
    std::cout << shader_pgm << std::endl;
    g_MemoryManager->Finalize();

    return 0;
}
