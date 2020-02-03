#include "AssetLoader.hpp"

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
}  // namespace My

using namespace My;
int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();

    g_pMemoryManager->Finalize();
    return 0;
}
