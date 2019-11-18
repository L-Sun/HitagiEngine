#pragma once
#include "IRuntimeModule.hpp"

namespace My {

class DebugManager : public IRuntimeModule {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    void ToggleDebugInfo();
    void DrawDebugInfo();

protected:
    bool m_bDrawDebugInfo = false;
};
extern DebugManager* g_pDebugManager;
}  // namespace My
