#include <iostream>
#include <string>
#include "MemoryManager.hpp"
#include "FileIOManager.hpp"

using namespace Hitagi;

int main(int argc, char const* argv[]) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();

    std::string shader_pgm = g_FileIOManager->SyncOpenAndReadTextFileToString("Asset/Shaders/basic.vs");
    std::cout << shader_pgm << std::endl;

    Core::Buffer buff = g_FileIOManager->SyncOpenAndReadBinary("Asset/Shaders/basic.vs");

    for (size_t i = 0; i < buff.GetDataSize(); i++) {
        std::cout << buff.GetData()[i];
    }
    std::cout << std::endl;

    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}
