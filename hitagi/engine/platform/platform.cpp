module platform;
import std;
#if defined(_WIN32)
import :win32;
#else
import :sdl;
#endif

namespace hitagi::platform {

auto PlatformHost::Create(PlatformConfig config) -> std::unique_ptr<PlatformHost> {
    std::unique_ptr<PlatformHost> result = nullptr;
#if defined(_WIN32)
    result = std::make_unique<Win32PlatformHost>(std::move(config));
#else
    result = std::make_unique<SDLPlatformHost>(std::move(config));
#endif
    return result;
}

}  // namespace hitagi::platform
