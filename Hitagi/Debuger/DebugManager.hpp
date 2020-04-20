#pragma once
#include "IRuntimeModule.hpp"

namespace Hitagi {

class DebugManager : public IRuntimeModule {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual ~DebugManager() {}

    void ToggleDebugInfo();
    void DrawDebugInfo();

protected:
    bool m_DrawDebugInfo = false;
};
extern std::unique_ptr<DebugManager> g_DebugManager;
}  // namespace Hitagi
