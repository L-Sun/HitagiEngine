#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "FileIOManager.hpp"

using namespace Hitagi;

namespace Hitagi {
std::unique_ptr<Core::MemoryManager> g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager> g_FileIOManager(new Core::FileIOManager);
}  // namespace Hitagi

int main(int argc, char const* argv[]) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

    std::string shader_pgm = g_FileIOManager->SyncOpenAndReadTextFileToString("Asset/Shaders/basic.vs");
    std::cout << shader_pgm << std::endl;

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}