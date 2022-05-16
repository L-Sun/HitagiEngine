#pragma once
#include <hitagi/graphics/enums.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/math/vector.hpp>

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
    using allocator_type = std::pmr::polymorphic_allocator<>;

    Resource(std::string_view name, std::unique_ptr<backend::Resource> resource, allocator_type alloc = {})
        : m_Name(name, alloc), m_Resource(std::move(resource)) {}
    Resource(const Resource&)            = delete;
    Resource& operator=(const Resource&) = delete;
    Resource(Resource&&)                 = default;
    Resource& operator=(Resource&&)      = default;

    inline std::string_view GetName() const noexcept { return m_Name; }
    inline std::uint32_t    Version() const noexcept { return m_Version; }

    template <typename T>
    inline T* GetBackend() const noexcept {
        using RT = std::remove_cv_t<T>;
        return static_cast<RT*>(m_Resource.get());
    }

protected:
    std::pmr::string                   m_Name;
    std::unique_ptr<backend::Resource> m_Resource;
    std::uint32_t                      m_Version;
};

template <typename Desc>
class ResourceWithDesc : public Resource {
public:
    ResourceWithDesc(std::string_view                   name,
                     std::unique_ptr<backend::Resource> res,
                     Desc                               desc,
                     allocator_type                     alloc = {})
        : Resource(name, std::move(res), desc, alloc), desc(desc) {}

    const Desc desc;
};

struct VertexBufferDesc {
    std::size_t vertex_count = 0;
    std::size_t vertex_size  = 0;
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
    Format         format;
    uint64_t       width;
    uint64_t       height;
    uint64_t       pitch;
    unsigned       mip_level         = 1;
    unsigned       sample_count      = 1;
    unsigned       sample_quality    = 0;
    const uint8_t* initial_data      = nullptr;
    size_t         initial_data_size = 0;
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