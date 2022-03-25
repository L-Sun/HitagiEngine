#pragma once
#include "types.hpp"

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
    Resource(std::string_view name, std::unique_ptr<backend::Resource> resource)
        : m_Name(name), m_Resource(std::move(resource)) {}
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource(Resource&&)                 = default;
    Resource& operator=(Resource&&) = default;

    inline const std::string& GetName() const noexcept { return m_Name; }

    template <typename T>
    inline T* GetBackend() const noexcept {
        using RT = std::remove_cv_t<T>;
        return static_cast<RT*>(m_Resource.get());
    }

protected:
    std::string                        m_Name;
    std::unique_ptr<backend::Resource> m_Resource;
};

class VertexBuffer : public Resource {
public:
    VertexBuffer(std::string_view name, std::unique_ptr<backend::Resource> resource, size_t vertex_count, size_t vertex_size)
        : Resource(name, std::move(resource)),
          vertex_count(vertex_count),
          vertex_size(vertex_size) {}

    const size_t vertex_count = 0;
    const size_t vertex_size  = 0;
};
class IndexBuffer : public Resource {
public:
    IndexBuffer(std::string_view name, std::unique_ptr<backend::Resource> resource, size_t index_count, size_t index_size)
        : Resource(name, std::move(resource)),
          index_count(index_count),
          index_size(index_size) {}

    const size_t index_count = 0;
    const size_t index_size  = 0;
};
struct MeshBuffer {
    std::unordered_map<std::string, std::shared_ptr<VertexBuffer>> vertices;
    std::shared_ptr<IndexBuffer>                                   indices;
    PrimitiveType                                                  primitive;
    std::optional<size_t>                                          index_count;
    std::optional<size_t>                                          index_offset;
    std::optional<size_t>                                          vertex_offset;
};
class ConstantBuffer : public Resource {
    friend class DriverAPI;

public:
    ConstantBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpu_resource, size_t num_elements, size_t element_size)
        : Resource(name, std::move(gpu_resource)), m_NumElements(num_elements), m_ElementSize(element_size) {}

    size_t GetNumElements() const noexcept { return m_NumElements; }
    size_t GetElementSize() const noexcept { return m_ElementSize; }

    void UpdateNumElements(size_t num_elements) noexcept { m_NumElements = num_elements; }

private:
    size_t m_NumElements = 0;
    size_t m_ElementSize = 0;
};

class TextureBuffer : public Resource {
public:
    struct Description {
        Format         format;
        uint64_t       width;
        uint64_t       height;
        uint64_t       pitch;
        unsigned       mip_level         = 1;
        unsigned       sample_count      = 1;
        unsigned       sample_quality    = 0;
        const uint8_t* initial_data      = nullptr;
        size_t         initial_data_size = 0;
    } const desc;

    TextureBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpu_resource, Description desc)
        : Resource(name, std::move(gpu_resource)), desc(desc) {}
};

class DepthBuffer : public Resource {
public:
    struct Description {
        Format   format;
        uint64_t width;
        uint64_t height;
        float    clear_depth;
        uint8_t  clear_stencil;
    } const desc;
    DepthBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpu_resource, Description desc)
        : Resource(name, std::move(gpu_resource)), desc(desc) {}
};

class RenderTarget : public Resource {
public:
    struct Description {
        Format   format;
        uint64_t width;
        uint64_t height;
    } const desc;
    RenderTarget(std::string_view name, std::unique_ptr<backend::Resource> gpu_resource, Description desc)
        : Resource(name, std::move(gpu_resource)), desc(desc) {}
};

class Sampler : public Resource {
public:
    struct Description {
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
    } const desc;
    Sampler(std::string_view name, std::unique_ptr<backend::Resource> sampler, Description desc)
        : Resource(name, std::move(sampler)), desc(std::move(desc)) {}
};

}  // namespace hitagi::graphics