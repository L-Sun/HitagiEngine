#pragma once
#include "IRuntimeModule.hpp"

namespace My {
class GraphicsManager : implements IRuntimeMoudle {
public:
    virtual ~GraphicsManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual void Draw();
    virtual void Clear();
};

extern GraphicsManager* g_pGraphicsManager;
}  // namespace My
