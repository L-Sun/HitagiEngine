module;
#include <tracy/Tracy.hpp>
#include <cstddef>
#include <new>

export module interop.tracy;

export namespace tracy {
using ::tracy::Color;
using ::tracy::SetThreadName;
#ifdef TRACY_ENABLE
using ::tracy::Lockable;
using ::tracy::MessageSeverity;
using ::tracy::MessageSourceType;
using ::tracy::PlotFormatType;
using ::tracy::Profiler;
using ::tracy::ScopedZone;
using ::tracy::SourceLocationData;
#endif
}  // namespace tracy

export namespace hitagi::interop {
#ifdef TRACY_ENABLE
inline constexpr bool profiling_enabled = true;
inline constexpr int profiling_callstack_depth = TRACY_CALLSTACK;
#else
inline constexpr bool profiling_enabled = false;
#endif
}  // namespace hitagi::interop

export namespace hitagi::interop {
#ifdef TRACY_ENABLE
// Keep the CPU Tracy implementation in this module when callers also import
// the Vulkan/DX12 Tracy headers through their separate bridge modules.
void plot_profiling_value(const char* name, double value) {
    tracy::Profiler::PlotData(name, value);
}
#endif
}  // namespace hitagi::interop

#ifdef TRACY_ENABLE
export namespace hitagi::interop {
// Keep Tracy inline implementation within one module, including destruction.
class ProfilingScope {
public:
    ProfilingScope(const tracy::SourceLocationData* location, int depth, bool active);
    ~ProfilingScope();
    void Name(const char* text, size_t size);
    void Text(const char* text, size_t size);
    ProfilingScope(const ProfilingScope&) = delete;
    ProfilingScope& operator=(const ProfilingScope&) = delete;
private:
    alignas(tracy::ScopedZone) unsigned char storage[sizeof(tracy::ScopedZone)];
};
}
namespace hitagi::interop {
ProfilingScope::ProfilingScope(const tracy::SourceLocationData* location, int depth, bool active) {
    new (storage) tracy::ScopedZone(location, depth, active);
}
ProfilingScope::~ProfilingScope() { reinterpret_cast<tracy::ScopedZone*>(storage)->~ScopedZone(); }
void ProfilingScope::Name(const char* text, size_t size) { reinterpret_cast<tracy::ScopedZone*>(storage)->Name(text, size); }
void ProfilingScope::Text(const char* text, size_t size) { reinterpret_cast<tracy::ScopedZone*>(storage)->Text(text, size); }
}
#endif
