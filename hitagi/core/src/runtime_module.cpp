#include <hitagi/core/runtime_module.hpp>

#include <spdlog/logger.h>

namespace hitagi {
std::string_view RuntimeModule::GetName() const {
    if (m_Logger != nullptr)
        return m_Logger->name();
    else
        return "UnNamed";
}

}  // namespace hitagi