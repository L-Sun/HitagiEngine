#include <hitagi/application.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/hid/input_manager.hpp>

#ifdef WIN32
#include "windows/win32_application.hpp"
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
Application* app = nullptr;

// Parse command line, read configuration, initialize all sub modules
bool Application::Initialize() {
    if (!input_manager) {
        input_manager = static_cast<decltype(input_manager)>(LoadModule(std::make_unique<hid::InputManager>()));
    }
    RuntimeModule::Initialize();
    m_Clock.Start();

    // Windows
    InitializeWindows();

    return true;
}

void Application::Tick() {
    m_Clock.Tick();
    RuntimeModule::Tick();
}

#ifdef WIN32
std::unique_ptr<Application> Application::CreateApp() {
    std::unique_ptr<Application> result = std::make_unique<Win32Application>();
    return result;
}
#endif
}  // namespace hitagi