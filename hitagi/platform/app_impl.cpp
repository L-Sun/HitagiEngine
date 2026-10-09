module app;
import std;
#if defined(_WIN32)
import :win32;
#else
import :sdl;
#endif

namespace hitagi {

auto Application::CreateApp(AppConfig config) -> std::unique_ptr<Application> {
    std::unique_ptr<Application> result = nullptr;
#if defined(_WIN32)
    result = std::make_unique<Win32Application>(std::move(config));
#else
    result = std::make_unique<SDL3Application>(std::move(config));
#endif
    return result;
}

}  // namespace hitagi
