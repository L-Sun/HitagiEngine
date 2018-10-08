#pragma once
#include "IRuntimeModule.hpp"

namespace My {
class GraphicsManager : implements IRuntimeMoudle {
public:
    virtual ~GraphicsManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
};
}  // namespace My
