#include <hitagi/application.hpp>
#include <hitagi/core/config_manager.hpp>

#ifdef WIN32
#include "windows/win32_application.hpp"
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
Application* app = nullptr;

// Parse command line, read configuration, initialize all sub modules
bool Application::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Application");

    m_Logger->info("Initialize Application");
    m_Clock.Start();

    // Windows
    InitializeWindows();

    return true;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void Application::Tick() {
    m_Clock.Tick();
}

#ifdef WIN32
std::unique_ptr<Application> Application::CreateApp() {
    std::unique_ptr<Application> result = std::make_unique<Win32Application>();
    return result;
}
#endif
}  // namespace hitagi