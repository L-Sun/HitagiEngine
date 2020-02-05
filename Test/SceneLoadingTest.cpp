#include <iostream>
#include <string>
#include "AssetLoader.hpp"
#include "MemoryManager.hpp"
#include "SceneManager.hpp"

using namespace My;

#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

namespace My {
std::unique_ptr<MemoryManager> g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>   g_pAssetLoader(new AssetLoader);
std::unique_ptr<SceneManager>  g_pSceneManager(new SceneManager);
}  // namespace My

template <typename T>
static std::ostream& operator<<(
    std::ostream&                                       out,
    std::unordered_map<std::string, std::shared_ptr<T>> map) {
    for (auto p : map) {
        out << *p.second << std::endl;
    }

    return out;
}

int main(int, char**) {
    g_pMemoryManager->Initialize();
    g_pSceneManager->Initialize();
    g_pAssetLoader->Initialize();

    g_pSceneManager->LoadScene("Asset/Scene/balls.ogex");
    auto& scene = g_pSceneManager->GetSceneForRendering();

    // std::cout << "Dump of Cameras" << std::endl;
    // std::cout << "---------------------------" << std::endl;
    // for (auto [key, pCameraNode] : scene.CameraNodes) {
    //     if (pCameraNode) {
    //         std::weak_ptr<SceneObjectCamera> pCamera =
    //             scene.GetCamera(pCameraNode->GetSceneObjectRef());
    //         if (auto pObj = pCamera.lock()) std::cout << *pObj << std::endl;
    //     }
    // }

    // std::cout << "Dump of Lights" << std::endl;
    // std::cout << "---------------------------" << std::endl;
    // for (auto [key, pLightNode] : scene.LightNodes) {
    //     if (pLightNode) {
    //         std::weak_ptr<SceneObjectLight> pLight =
    //             scene.GetLight(pLightNode->GetSceneObjectRef());
    //         if (auto pObj = pLight.lock()) std::cout << *pObj << std::endl;
    //     }
    // }

    // std::cout << "Dump of Geometries" << std::endl;
    // std::cout << "---------------------------" << std::endl;
    // for (auto [key, pGeometryNode] : scene.GeometryNodes) {
    //     if (pGeometryNode) {
    //         std::weak_ptr<SceneObjectGeometry> pGeometry =
    //             scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
    //         if (auto pObj = pGeometry.lock()) std::cout << *pObj <<
    //         std::endl;
    //     }
    // }

    // std::cout << "Dump of Materials" << std::endl;
    // std::cout << "---------------------------" << std::endl;
    // for (auto [key, pMaterial] : scene.Materials) {
    //     if (pMaterial) std::cout << *pMaterial << std::endl;
    // }
    g_pSceneManager->ResetScene();
    g_pSceneManager->Finalize();
    g_pAssetLoader->Finalize();
    g_pMemoryManager->Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
