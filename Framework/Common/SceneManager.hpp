#pragma once
#include "geommath.hpp"
#include "IRuntimeModule.hpp"
#include "SceneParser.hpp"

namespace My {

class SceneManager : implements IRuntimeModule {
public:
    virtual ~SceneManager();
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    int LoadScene(const char* scene_file_name);

    bool         IsSceneChanged();
    const Scene& GetSceneForRendering();

protected:
    bool LoadOgexScene(const char* ogex_scene_file_name);

    std::unique_ptr<Scene> m_pScene;
    bool                   m_bDirtyFlag = false;
};

extern SceneManager* g_pSceneManager;
}  // namespace My