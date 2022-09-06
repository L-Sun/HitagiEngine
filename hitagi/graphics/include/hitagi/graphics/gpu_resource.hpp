#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/shader.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/sampler.hpp>
#include <hitagi/graphics/enums.hpp>

#include <bitset>
#include <memory>
#include <unordered_map>
#include <string>
#include <optional>

namespace hitagi::graphics {
struct ConstantBuffer : public resource::Resource {
    std::size_t                                                     num_elements = 0;
    std::size_t                                                     element_size = 0;
    std::function<void(std::size_t, const std::byte*, std::size_t)> update_fn;
    std::function<void(std::size_t)>                                resize_fn;

    template <typename T>
    inline void Update(std::size_t index, const T& element) {
        update_fn(index, reinterpret_cast<const std::byte*>(&element), sizeof(T));
    }
    inline void Update(std::size_t index, const std::byte* data, std::size_t size) {
        update_fn(index, data, size);
    }
    inline void Resize(std::size_t num_elements) {
        resize_fn(num_elements);
    }
};

struct PipelineState : public resource::Resource {
    std::shared_ptr<resource::Shader>   vs                  = nullptr;
    std::shared_ptr<resource::Shader>   ps                  = nullptr;
    resource::PrimitiveType             primitive_type      = resource::PrimitiveType::TriangleList;
    BlendDescription                    blend_state         = {};
    RasterizerDescription               rasterizer_state    = {};
    resource::Format                    render_format       = resource::Format::R8G8B8A8_UNORM;
    resource::Format                    depth_buffer_format = resource::Format::UNKNOWN;
    std::pmr::vector<resource::Sampler> static_samplers;
};

}  // namespace hitagi::graphics