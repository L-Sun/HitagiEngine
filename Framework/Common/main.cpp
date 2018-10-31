#include <iostream>
#include <chrono>
#include <thread>
#include "BaseApplication.hpp"

using namespace My;

int main(int argc, char const* argv[]) {
    int ret;

    std::cout << "\n------- Initial App -------" << std::endl;
    if ((ret = g_pApp->Initialize()) != 0) {
        std::cout << "App Initialize failed, will exit now." << std::endl;
        return ret;
    }

    std::cout << "\n------- Initial Memory Manager -------" << std::endl;
    if ((ret = g_pMemoryManager->Initialize()) != 0) {
        std::cout << "Memory Manager Initialize failed, will exit now."
                  << std::endl;
        return ret;
    }

    std::cout << "\n------- Initial Scene Manager -------" << std::endl;
    if ((ret = g_pSceneManager->Initialize()) != 0) {
        std::cout << "Scene Manager Initialize failed, will exit now."
                  << std::endl;
        return ret;
    }

    std::cout << "\n------- Initial Asset Loader -------" << std::endl;
    if ((ret = g_pAssetLoader->Initialize()) != 0) {
        std::cout << "Asset Loader Initialize failed, will exit now."
                  << std::endl;
        return ret;
    }
    g_pSceneManager->LoadScene("Scene/cube.ogex");

    std::cout << "\n------- Initial Graphics Manager -------" << std::endl;
    if ((ret = g_pGraphicsManager->Initialize()) != 0) {
        std::cout << "Graphics Manager Initialize failed, will exit now."
                  << std::endl;
        return ret;
    }

    while (!g_pApp->IsQuit()) {
        g_pApp->Tick();
        g_pMemoryManager->Tick();
        g_pAssetLoader->Tick();
        g_pSceneManager->Tick();
        g_pGraphicsManager->Tick();
        std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }

    g_pAssetLoader->Finalize();
    g_pSceneManager->Finalize();
    g_pGraphicsManager->Finalize();
    g_pMemoryManager->Finalize();
    g_pApp->Finalize();

    return 0;
}
