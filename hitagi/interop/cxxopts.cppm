module;
#include <cxxopts.hpp>

export module interop.cxxopts;

export namespace cxxopts {
using ::cxxopts::Options;
using ::cxxopts::ParseResult;
using ::cxxopts::value;
}  // namespace cxxopts
