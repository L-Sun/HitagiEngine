#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/asset/resource.hpp>
#include <hitagi/asset/enums.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/utils/utils.hpp>

#include <filesystem>

namespace hitagi::asset {
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
    struct DepthStencil {
        float         depth;
        std::uint32_t stencil;
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

    std::variant<math::vec4f, DepthStencil> clear_value;

    std::filesystem::path path;
};

}  // namespace hitagi::asset

template <>
struct hitagi::utils::enable_bitmask_operators<hitagi::asset::Texture::BindFlag> {
    static constexpr bool enable = true;
};