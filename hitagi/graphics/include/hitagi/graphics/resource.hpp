#pragma once
#include <hitagi/math/vector.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/graphics/enums.hpp>

#include <bitset>
#include <memory>
#include <unordered_map>
#include <string>
#include <optional>

namespace hitagi::graphics {

namespace backend {
class Resource {
public:
    virtual ~Resource() = default;
};
}  // namespace backend

class Resource {
public:
    Resource(std::string_view name, std::unique_ptr<backend::Resource> resource)
        : m_Name(name), m_Resource(std::move(resource)) {}

    Resource(const Resource&)  = delete;
    Resource(Resource&& other) = default;

    Resource& operator=(const Resource&) = delete;
    Resource& operator=(Resource&&) = default;

    inline std::string_view GetName() const noexcept { return m_Name; }

    inline void SetResource(std::unique_ptr<backend::Resource> resource) noexcept { m_Resource = std::move(resource); }

    template <typename T>
    inline T* GetBackend() const noexcept {
        using RT = std::remove_cv_t<T>;
        return static_cast<RT*>(m_Resource.get());
    }

protected:
    std::pmr::string                   m_Name;
    std::unique_ptr<backend::Resource> m_Resource;
};

template <typename Desc>
class ResourceWithDesc : public Resource {
public:
    using DescType = Desc;
    ResourceWithDesc(std::string_view                   name,
                     std::unique_ptr<backend::Resource> res,
                     Desc                               desc)
        : Resource(name, std::move(res)), desc(desc) {}

    const Desc desc;
};

struct VertexBufferDesc {
    std::size_t vertex_count = 0;
    //  indicate which slot is enabled
    std::bitset<magic_enum::enum_count<resource::VertexAttribute>()>             slot_mask;
    std::array<std::size_t, magic_enum::enum_count<resource::VertexAttribute>()> slot_offset;
};
using VertexBuffer = ResourceWithDesc<VertexBufferDesc>;

struct IndexBufferDesc {
    std::size_t index_count = 0;
    std::size_t index_size  = 0;
};
using IndexBuffer = ResourceWithDesc<IndexBufferDesc>;

struct ConstantBufferDesc {
    std::size_t num_elements = 0;
    std::size_t element_size = 0;
};
using ConstantBuffer = ResourceWithDesc<ConstantBufferDesc>;

struct TextureBufferDesc {
    Format   format;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    unsigned mip_level      = 1;
    unsigned sample_count   = 1;
    unsigned sample_quality = 0;
};
using TextureBuffer = ResourceWithDesc<TextureBufferDesc>;

struct DepthBufferDesc {
    Format   format;
    uint64_t width;
    uint64_t height;
    float    clear_depth;
    uint8_t  clear_stencil;
};
using DepthBuffer = ResourceWithDesc<DepthBufferDesc>;

struct RenderTargetDesc {
    Format   format;
    uint64_t width;
    uint64_t height;
};
using RenderTarget = ResourceWithDesc<RenderTargetDesc>;

struct SamplerDesc {
    Filter             filter         = Filter::ANISOTROPIC;
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
using Sampler = ResourceWithDesc<SamplerDesc>;

}  // namespace hitagi::graphics