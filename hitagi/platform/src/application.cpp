#include <hitagi/application.hpp>

#ifdef WIN32
#include "windows/win32_application.hpp"
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
Application* app = nullptr;

bool Application::sm_Quit = false;

// Parse command line, read configuration, initialize all sub modules
bool Application::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Application");

    m_Logger->info("Initialize Application");

    // Windows
    InitializeWindows();

    return true;
}

// Finalize all sub modules and clean up all runtime temporary files.
void Application::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void Application::Tick() {}

void Application::SetCommandLineParameters(int argc, char** argv) {
    m_ArgSize = argc;
    m_Arg     = argv;
}

bool Application::IsQuit() { return sm_Quit; }

#ifdef WIN32
std::unique_ptr<Application> Application::CreateApp() {
    std::unique_ptr<Application> result = std::make_unique<Win32Application>();
    return result;
}
#endif
}  // namespace hitagi