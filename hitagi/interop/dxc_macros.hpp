#pragma once

#ifdef _WIN32
#include "win32_macros.hpp"
#else
// Match DXC's WinAdapter: deduce the interface without evaluating ppType,
// then evaluate the pointer once for the output argument.
#define IID_PPV_ARGS(ppType) \
    ::hitagi::interop::dxc_uuidof<std::remove_cvref_t<decltype(**(ppType))>>(), reinterpret_cast<void**>(ppType)
#endif
