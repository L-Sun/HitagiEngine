#include "SceneManager.hpp"
#include "FileIOManager.hpp"
#include "Assimp.hpp"

namespace Hitagi::Resource {

SceneManager::~SceneManager() {}

int SceneManager::Initialize() {
    int result = 0;
    m_Scene    = std::make_unique<Scene>();
    return result;
}
void SceneManager::Finalize() { m_Scene = nullptr; }

void SceneManager::Tick() {
    if (IsSceneChanged()) {
    }
}

int SceneManager::LoadScene(std::filesystem::path sceneFile) {
    m_ScenePath      = sceneFile;
    Core::Buffer buf = g_FileIOManager->SyncOpenAndReadBinary(sceneFile);
    AssimpParser parser;
    m_Scene = parser.Parse(buf);
    if (m_Scene) {
        m_Scene->LoadResource();
        m_DirtyFlag = true;
        return 0;
    }
    return -1;
}

void SceneManager::ResetScene() { m_DirtyFlag = true; }

const Scene& SceneManager::GetSceneForRendering() const { return *m_Scene; }

const Scene& SceneManager::GetSceneForPhysicsSimulation() const { return *m_Scene; }

bool SceneManager::IsSceneChanged() { return m_DirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_DirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(const std::string& name) {
    auto it = m_Scene->GeometryNodes.find(name);
    if (it != m_Scene->GeometryNodes.end())
        return it->second;
    else
        return std::weak_ptr<SceneGeometryNode>();
}
std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(const std::string& key) {
    return m_Scene->Geometries.find(key)->second;
}
}  // namespace Hitagi::Resource