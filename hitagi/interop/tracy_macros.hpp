#pragma once

// Import interop.tracy (and the GPU-specific module when needed) after the
// module declaration. This header intentionally includes no third-party code.
// Keep scope ownership, static source locations and disabled argument elision
// at the call site; types and operations come from the compiled Tracy modules.

#define HITAGI_TRACY_JOIN_INNER(a, b) a##b
#define HITAGI_TRACY_JOIN(a, b) HITAGI_TRACY_JOIN_INNER(a, b)

#ifdef TRACY_ENABLE

#define HITAGI_TRACY_SCOPE(name, depth) \
    static constexpr ::tracy::SourceLocationData HITAGI_TRACY_JOIN(hitagi_tracy_location_, __LINE__){name, __FUNCTION__, __FILE__, __LINE__, 0}; \
    ::tracy::ScopedZone hitagi_tracy_zone{&HITAGI_TRACY_JOIN(hitagi_tracy_location_, __LINE__), depth, true}
#define ZoneScoped HITAGI_TRACY_SCOPE(nullptr, ::hitagi::interop::profiling_callstack_depth)
#define ZoneScopedN(name) HITAGI_TRACY_SCOPE(name, ::hitagi::interop::profiling_callstack_depth)
#define ZoneScopedNS(name, depth) HITAGI_TRACY_SCOPE(name, depth)
#define ZoneName(name, size) hitagi_tracy_zone.Name(name, size)
#define ZoneText(text, size) hitagi_tracy_zone.Text(text, size)
#define FrameMark ::tracy::Profiler::SendFrameMark(nullptr)
#define TracyLockableN(type, variable, description) \
    ::tracy::Lockable<type> variable{[]() -> const ::tracy::SourceLocationData* { \
        static constexpr ::tracy::SourceLocationData location{nullptr, description, __FILE__, __LINE__, 0}; \
        return &location; \
    }()}
#define TracyPlot(name, value) ::tracy::Profiler::PlotData(name, value)
#define TracyPlotConfig(name, type, step, fill, color) ::tracy::Profiler::ConfigurePlot(name, type, step, fill, color)
#define TracyAllocN(pointer, size, name) ::tracy::Profiler::MemAllocCallstackNamed(pointer, size, ::hitagi::interop::profiling_callstack_depth, false, name)
#define TracyFreeN(pointer, name) ::tracy::Profiler::MemFreeCallstackNamed(pointer, ::hitagi::interop::profiling_callstack_depth, false, name)
#define TracyMessageCS(text, size, color, depth) ::tracy::Profiler::MessageColor(text, size, color, depth)
#define TracyMessageLCS(text, color, depth) ::tracy::Profiler::MessageColor(text, color, depth)

#define TracyVkContext(physical_device, device, queue, command_buffer) ::hitagi::interop::create_tracy_context(physical_device, device, queue, command_buffer)
#define TracyVkDestroy(context) ::hitagi::interop::destroy_tracy_context(context)
#define TracyVkContextName(context, name, size) (context)->Name(name, size)
#define TracyVkCollect(context, command_buffer) (context)->Collect(command_buffer)
#define TracyD3D12Context(device, queue) ::hitagi::interop::create_tracy_context(device, queue)
#define TracyD3D12Destroy(context) ::hitagi::interop::destroy_tracy_context(context)
#define TracyD3D12ContextName(context, name, size) (context)->Name(name, size)
#define TracyD3D12NewFrame(context) (context)->NewFrame()
#define TracyD3D12Collect(context) (context)->Collect()

#else

#define ZoneScoped
#define ZoneScopedN(name)
#define ZoneScopedNS(name, depth)
#define ZoneName(name, size)
#define ZoneText(text, size)
#define FrameMark
#define TracyLockableN(type, variable, description) type variable
#define TracyPlot(name, value)
#define TracyPlotConfig(name, type, step, fill, color)
#define TracyAllocN(pointer, size, name)
#define TracyFreeN(pointer, name)
#define TracyMessageCS(text, size, color, depth)
#define TracyMessageLCS(text, color, depth)
#define TracyVkContext(physical_device, device, queue, command_buffer) nullptr
#define TracyVkDestroy(context)
#define TracyVkContextName(context, name, size)
#define TracyVkCollect(context, command_buffer)
#define TracyD3D12Context(device, queue) nullptr
#define TracyD3D12Destroy(context)
#define TracyD3D12ContextName(context, name, size)
#define TracyD3D12NewFrame(context)
#define TracyD3D12Collect(context)

#endif
