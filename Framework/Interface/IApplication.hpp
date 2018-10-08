#pragma once
#include "IRuntimeModule.hpp"
#include "GfxConfiguration.h"

namespace My {
Interface IApplication : implements IRuntimeMoudle {
public:
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;
    virtual bool IsQuit()     = 0;

    // virtual GfxConfiguration& GetConfiguration() = 0;
};
}  // namespace My
