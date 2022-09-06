#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/resource.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/utils/utils.hpp>

#include <filesystem>

namespace hitagi::resource {
enum struct Format : std::uint32_t;

// TODO support 1D and 3D texture
struct Texture : public Resource {
    enum struct BindFlag {
        None            = 0,
        ShaderResource  = 1 << 0,
        UnorderedAccess = 1 << 1,
        RenderTarget    = 1 << 2,
        DepthBuffer     = 1 << 3,
    };

    BindFlag      bind_flags = BindFlag::ShaderResource;
    Format        format     = Format::UNKNOWN;
    std::uint32_t width      = 1;
    std::uint32_t height     = 1;
    std::uint32_t pitch      = 1;

    std::uint32_t array_size     = 1;
    std::uint32_t mip_level      = 1;
    std::uint32_t sample_count   = 1;
    std::uint32_t sample_quality = 0;

    union {
        hitagi::math::vec4f color = {0, 0, 0, 1};
        struct {
            float         depth;
            std::uint32_t stencil;
        } depth_stencil;
    } clear_value;

    std::filesystem::path path;
};

}  // namespace hitagi::resource

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::resource::Texture::BindFlag> {
    static constexpr bool enable = true;
};