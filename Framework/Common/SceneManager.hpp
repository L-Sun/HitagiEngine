#pragma once
#include "IRuntimeModule.hpp"
#include "SceneNode.hpp"

namespace My {

class SceneManager : implements IRuntimeMoudle {
public:
    virtual ~SceneManager();
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

protected:
    SceneEmptyNode m_RootNode;
};
}  // namespace My