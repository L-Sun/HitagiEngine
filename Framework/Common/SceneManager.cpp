#include "SceneManager.hpp"
#include "AssetLoader.hpp"
#include "OGEX.hpp"

using namespace My;

SceneManager::~SceneManager() {}

int SceneManager::Initialize() {
    int result = 0;
    m_pScene   = make_shared<Scene>();
    return result;
}
void SceneManager::Finalize() {}

void SceneManager::Tick() {}

int SceneManager::LoadScene(const char* scene_file_name) {
    if (LoadOgexScene(scene_file_name)) {
        m_pScene->LoadResource();
        m_bDirtyFlag = true;
        return 0;
    } else {
        return -1;
    }
}

void SceneManager::ResetScene() { m_bDirtyFlag = true; }

bool SceneManager::LoadOgexScene(const char* ogex_scene_file_name) {
    OgexParser ogex_parser;
    m_pScene = ogex_parser.Parse(ogex_scene_file_name);
    if (!m_pScene) {
        return false;
    }
    return true;
}

const Scene& SceneManager::GetSceneForRendering() const { return *m_pScene; }

const Scene& SceneManager::GetSceneForPhysicsSimulation() const {
    return *m_pScene;
}

bool SceneManager::IsSceneChanged() { return m_bDirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_bDirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(
    std::string name) {
    auto it = m_pScene->LUT_Name_GeometryNode.find(name);
    if (it != m_pScene->LUT_Name_GeometryNode.end())
        return it->second;
    else
        return weak_ptr<SceneGeometryNode>();
}
std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(
    std::string key) {
    return m_pScene->Geometries.find(key)->second;
}
