#pragma once
#include <streambuf>
#include <istream>

namespace hitagi::utils {
struct membuf : std::streambuf {
    membuf(const char* base, std::size_t size) {
        char* p(const_cast<char*>(base));
        this->setg(p, p, p + size);
    }
    membuf(const std::byte* base, std::size_t size) : membuf(reinterpret_cast<const char*>(base), size) {}
};

struct imemstream : virtual membuf, std::istream {
    imemstream(const char* base, std::size_t size)
        : membuf(base, size), std::istream(static_cast<std::streambuf*>(this)) {
    }
    imemstream(const std::byte* base, std::size_t size) : imemstream(reinterpret_cast<char const*>(base), size) {}
};
}  // namespace hitagi::utils