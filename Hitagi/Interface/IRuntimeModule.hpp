#pragma once
#include <memory>

namespace spdlog {
class logger;
}

namespace Hitagi {
class IRuntimeModule {
public:
    virtual ~IRuntimeModule(){};
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

#if defined(_DEBUG)
    virtual void DrawDebugInfo(){};
#endif
protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace Hitagi
