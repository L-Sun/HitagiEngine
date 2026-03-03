module;

#if defined(USE_ISPC)
#include "ispc_math.hpp"
#endif  // USE_ISPC

export module math;
export import :vector;
export import :matrix;
export import :transform;
export import :geometry;
import std;

// The _deg literal must be in global namespace
export constexpr double operator""_deg(long double angle) {
    return angle / 180.0 * std::numbers::pi;
}
