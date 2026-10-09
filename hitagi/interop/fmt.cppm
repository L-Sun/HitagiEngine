module;
#include <fmt/color.h>

export module interop.fmt;

// Export the original entities without changing their ABI or ownership.
export namespace fmt {
using ::fmt::color;
using ::fmt::fg;
using ::fmt::format;
using ::fmt::ptr;
using ::fmt::rgb;
using ::fmt::styled;
}  // namespace fmt
