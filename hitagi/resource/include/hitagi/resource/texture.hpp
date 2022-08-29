#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/resource.hpp>
#include <hitagi/resource/enums.hpp>

#include <filesystem>

namespace hitagi::resource {
enum struct Format : std::uint32_t;

struct Texture : public Resource {
    Format        format;
    std::uint64_t width;
    std::uint64_t height;
    std::uint64_t pitch;
    unsigned      mip_level      = 1;
    unsigned      sample_count   = 1;
    unsigned      sample_quality = 0;

    std::filesystem::path path;
};

}  // namespace hitagi::resource