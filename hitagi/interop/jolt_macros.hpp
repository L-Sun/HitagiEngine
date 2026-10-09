#pragma once

// Match Jolt/Core/Core.h and IssueReporting.h, including JPH_NO_DEBUG.
#if defined(JPH_ENABLE_ASSERTS) || defined(JPH_DEBUG) || (!defined(NDEBUG) && !defined(JPH_NO_DEBUG))
#define JPH_IF_ENABLE_ASSERTS(...) __VA_ARGS__
#else
#define JPH_IF_ENABLE_ASSERTS(...)
#endif
