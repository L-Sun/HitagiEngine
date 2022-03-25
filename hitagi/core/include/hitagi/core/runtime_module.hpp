#pragma once
#include <memory>

namespace spdlog {
class logger;
}

namespace hitagi {
class IRuntimeModule {
public:
    virtual ~IRuntimeModule() = default;
    virtual int  Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace hitagi
