#include "SceneManager.hpp"
#include "AssetLoader.hpp"
#include "Assimp.hpp"

using namespace My;

SceneManager::~SceneManager() {}

int SceneManager::Initialize() {
    int result = 0;
    m_pScene   = std::make_unique<Scene>();
    return result;
}
void SceneManager::Finalize() { m_pScene = nullptr; }

void SceneManager::Tick() {
    if (IsSceneChanged()) {
        LoadScene(m_scenePath);
    }
}

int SceneManager::LoadScene(std::filesystem::path sceneFile) {
    m_scenePath      = sceneFile;
    Buffer       buf = g_pAssetLoader->SyncOpenAndReadBinary(sceneFile);
    AssimpParser parser;
    m_pScene = parser.Parse(buf);
    if (m_pScene) {
        m_pScene->LoadResource();
        m_bDirtyFlag = true;
        return 0;
    }
    return -1;
}

void SceneManager::ResetScene() { m_bDirtyFlag = true; }

const Scene& SceneManager::GetSceneForRendering() const { return *m_pScene; }

const Scene& SceneManager::GetSceneForPhysicsSimulation() const { return *m_pScene; }

bool SceneManager::IsSceneChanged() { return m_bDirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_bDirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(const std::string& name) {
    auto it = m_pScene->GeometryNodes.find(name);
    if (it != m_pScene->GeometryNodes.end())
        return it->second;
    else
        return std::weak_ptr<SceneGeometryNode>();
}
std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(const std::string& key) {
    return m_pScene->Geometries.find(key)->second;
}
