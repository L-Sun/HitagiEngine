#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>

#include <iostream>
#include <string>

using namespace hitagi;

auto main(int argc, char const* argv[]) -> int {
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();

    auto buffer = g_FileIoManager->SyncOpenAndReadBinary("Assets/Shaders/basic.vs");

    for (size_t i = 0; i < buffer.GetDataSize(); i++) {
        std::cout << buffer.GetData()[i];
    }
    std::cout << std::endl;

    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

    return 0;
}
