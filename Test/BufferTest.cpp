#include "AssetLoader.hpp"

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
}  // namespace My

using namespace My;
int main(int argc, char const* argv[]) {
    g_pMemoryManager->Initialize();
    std::vector<const void*> p;
    while (p.size() < 100000000000) {
        p.push_back(test.GetData());
    }

    g_pMemoryManager->Finalize();
    return 0;
}
