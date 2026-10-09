module;
#include <tracy/Tracy.hpp>

export module interop.tracy;

export namespace tracy {
using ::tracy::Color;
using ::tracy::SetThreadName;
#ifdef TRACY_ENABLE
using ::tracy::Lockable;
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
