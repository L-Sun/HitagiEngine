#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/enums.hpp>

#include <filesystem>

namespace hitagi::resource {
struct Image {
    Format                format;
    std::uint64_t         width;
    std::uint64_t         height;
    std::uint64_t         pitch;
    core::Buffer          data;
    std::filesystem::path path;
};
}  // namespace hitagi::resource