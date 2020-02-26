#pragma once
#include <memory>
namespace My {
class IRuntimeModule {
public:
    virtual ~IRuntimeModule(){};
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

#if defined(_DEBUG)
    virtual void DrawDebugInfo(){};
#endif
};
}  // namespace My
