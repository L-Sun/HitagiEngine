#include "MemoryManager.hpp"
#include "AssetManager.hpp"
#include "SceneManager.hpp"

using namespace Hitagi;

#define _CRTDBG_MAP_ALLOC
#ifdef _WIN32
#include <crtdbg.h>
#ifdef _DEBUG

#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#endif

template <typename T>
static std::ostream& operator<<(std::ostream& out, std::unordered_map<std::string, std::shared_ptr<T>> map) {
    for (auto p : map) {
        out << *p.second << std::endl;
    }

    return out;
}

int main(int, char**) {
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();
    g_AssetManager->Initialize();
    g_SceneManager->Initialize();

    auto scene = g_AssetManager->ImportScene("Asset/Scene/spot.fbx");

    std::cout << *scene->scene_graph << std::endl;

    std::cout << "Dump of Cameras" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pCameraNode] : scene->camera_nodes) {
        if (pCameraNode) {
            std::weak_ptr<Asset::SceneObjectCamera> p_camera = pCameraNode->GetSceneObjectRef();
            if (auto p_obj = p_camera.lock()) std::cout << *p_obj << std::endl;
        }
    }

    std::cout << "Dump of Lights" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pLightNode] : scene->light_nodes) {
        if (pLightNode) {
            std::weak_ptr<Asset::SceneObjectLight> p_light = pLightNode->GetSceneObjectRef();
            if (auto p_obj = p_light.lock()) std::cout << *p_obj << std::endl;
        }
    }

    std::cout << "Dump of Geometries" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pGeometryNode] : scene->geometry_nodes) {
        if (pGeometryNode) {
            std::weak_ptr<Asset::SceneObjectGeometry> p_geometry = pGeometryNode->GetSceneObjectRef();
            if (auto p_obj = p_geometry.lock()) std::cout << *p_obj << std::endl;
            std::cout << pGeometryNode->GetCalculatedTransform() << std::endl;
        }
    }

    std::cout << "Dump of Materials" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto [key, pMaterial] : scene->materials) {
        if (pMaterial) std::cout << *pMaterial << std::endl;
    }
    g_SceneManager->Finalize();
    g_AssetManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
