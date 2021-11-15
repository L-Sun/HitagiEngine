#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "FileIOManager.hpp"

using namespace Hitagi;

auto main(int argc, char const* argv[]) -> int {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

    auto buffer = g_FileIOManager->SyncOpenAndReadBinary("Asset/Shaders/basic.vs");

    for (size_t i = 0; i < buffer.GetDataSize(); i++) {
        std::cout << buffer.GetData()[i];
    }
    std::cout << std::endl;

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}
