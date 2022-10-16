#pragma once
#include <hitagi/asset/enums.hpp>
#include <hitagi/asset/resource.hpp>
#include <hitagi/math/vector.hpp>

namespace hitagi::asset {
struct Sampler : public Resource {
    Filter             filter         = Filter::Anisotropic;
    TextureAddressMode address_u      = TextureAddressMode::Wrap;
    TextureAddressMode address_v      = TextureAddressMode::Wrap;
    TextureAddressMode address_w      = TextureAddressMode::Wrap;
    float              mip_lod_bias   = 0;
    unsigned           max_anisotropy = 16;
    ComparisonFunc     comp_func      = ComparisonFunc::LessEqual;
    math::vec4f        border_color   = math::vec4f(0.0f);
    float              min_lod        = 0.0f;
    float              max_lod        = std::numeric_limits<float>::max();
};

}  // namespace hitagi::asset