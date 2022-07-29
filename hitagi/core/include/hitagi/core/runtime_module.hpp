#pragma once
#include <memory>
#include <string_view>

namespace spdlog {
class logger;
}

namespace hitagi {
class RuntimeModule {
public:
    virtual ~RuntimeModule()  = default;
    virtual bool Initialize() = 0;
    virtual void Finalize()   = 0;
    virtual void Tick()       = 0;

    virtual std::string_view GetName() const;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace hitagi
