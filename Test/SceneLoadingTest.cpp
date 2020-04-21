#include "MemoryManager.hpp"
#include "ResourceManager.hpp"
#include "SceneManager.hpp"

using namespace Hitagi;

#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

namespace Hitagi {
std::unique_ptr<Core::MemoryManager>       g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager>       g_FileIOManager(new Core::FileIOManager);
std::unique_ptr<Resource::SceneManager>    g_SceneManager(new Resource::SceneManager);
std::unique_ptr<Resource::ResourceManager> g_ResourceManager(new Resource::ResourceManager);
}  // namespace Hitagi

template <typename T>
static std::ostream& operator<<(std::ostream& out, std::unordered_map<std::string, std::shared_ptr<T>> map) {
    for (auto p : map) {
        out << *p.second << std::endl;
    }

    return out;
}

int main(int, char**) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();
    g_ResourceManager->Initialize();
    g_SceneManager->Initialize();

    g_SceneManager->SetScene("Asset/Scene/test.fbx");
    auto& scene = g_SceneManager->GetSceneForRendering();

    std::cout << *scene.SceneGraph << std::endl;

    std::cout << "Dump of Cameras" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pCameraNode] : scene.CameraNodes) {
        if (pCameraNode) {
            std::weak_ptr<Resource::SceneObjectCamera> pCamera = scene.GetCamera(pCameraNode->GetSceneObjectRef());
            if (auto pObj = pCamera.lock()) std::cout << *pObj << std::endl;
        }
    }

    std::cout << "Dump of Lights" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pLightNode] : scene.LightNodes) {
        if (pLightNode) {
            std::weak_ptr<Resource::SceneObjectLight> pLight = scene.GetLight(pLightNode->GetSceneObjectRef());
            if (auto pObj = pLight.lock()) std::cout << *pObj << std::endl;
        }
    }

    std::cout << "Dump of Geometries" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pGeometryNode] : scene.GeometryNodes) {
        if (pGeometryNode) {
            std::weak_ptr<Resource::SceneObjectGeometry> pGeometry =
                scene.GetGeometry(pGeometryNode->GetSceneObjectRef());
            if (auto pObj = pGeometry.lock()) std::cout << *pObj << std::endl;
            std::cout << *pGeometryNode->GetCalculatedTransform() << std::endl;
        }
    }

    std::cout << "Dump of Materials" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pMaterial] : scene.Materials) {
        if (pMaterial) std::cout << *pMaterial << std::endl;
    }
    g_SceneManager->Finalize();
    g_ResourceManager->Finalize();
    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
