#include "MemoryManager.hpp"
#include "AssetManager.hpp"

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

    auto scene = g_AssetManager->ImportScene("Assets/Scene/untitled.fbx");

    std::cout << scene->scene_graph << std::endl;

    std::cout << "Dump of Cameras" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto pCameraNode : scene->camera_nodes) {
        if (pCameraNode) {
            std::weak_ptr<Asset::Camera> p_camera = pCameraNode->GetSceneObjectRef();
            if (auto p_obj = p_camera.lock()) std::cout << *p_obj << std::endl;
        }
    }

    std::cout << "Dump of Lights" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto pLightNode : scene->light_nodes) {
        if (pLightNode) {
            std::weak_ptr<Asset::Light> p_light = pLightNode->GetSceneObjectRef();
            if (auto p_obj = p_light.lock()) std::cout << *p_obj << std::endl;
        }
    }

    std::cout << "Dump of Geometries" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto pGeometryNode : scene->geometry_nodes) {
        if (pGeometryNode) {
            std::weak_ptr<Asset::Geometry> p_geometry = pGeometryNode->GetSceneObjectRef();
            if (auto p_obj = p_geometry.lock()) std::cout << *p_obj << std::endl;
            std::cout << pGeometryNode->GetCalculatedTransformation() << std::endl;
        }
    }

    std::cout << "Dump of Materials" << std::endl;
    std::cout << "---------------------------" << std::endl;
    for (auto pMaterial : scene->materials) {
        if (pMaterial) std::cout << *pMaterial << std::endl;
    }
    g_AssetManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

#ifdef _WIN32
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif
    return 0;
}
