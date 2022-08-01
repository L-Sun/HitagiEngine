#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/shader.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/graphics/enums.hpp>

#include <bitset>
#include <memory>
#include <unordered_map>
#include <string>
#include <optional>

namespace hitagi::graphics {
struct ConstantBuffer : public resource::Resource {
    std::size_t num_elements = 0;
    std::size_t element_size = 0;
};

struct DepthBuffer : public resource::Resource {
    resource::Format format;
    std::uint64_t    width;
    std::uint64_t    height;
    float            clear_depth;
    std::uint8_t     clear_stencil;
};

struct RenderTarget : public resource::Resource {
    resource::Format format;
    std::uint64_t    width;
    std::uint64_t    height;
};

struct Sampler : public resource::Resource {
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

struct PipelineState : public resource::Resource {
    std::shared_ptr<resource::Shader> vs                      = nullptr;
    std::shared_ptr<resource::Shader> ps                      = nullptr;
    resource::PrimitiveType           primitive_type          = resource::PrimitiveType::TriangleList;
    BlendDescription                  blend_state             = {};
    RasterizerDescription             rasterizer_state        = {};
    resource::Format                  render_format           = resource::Format::R8G8B8A8_UNORM;
    resource::Format                  depth_buffer_format     = resource::Format::UNKNOWN;
    bool                              front_counter_clockwise = true;
    std::pmr::vector<Sampler>         static_samplers;
};

}  // namespace hitagi::graphics