#pragma once
#include "Format.hpp"
#include "Primitive.hpp"

#include <memory>
#include <unordered_map>
#include <string>

namespace Hitagi::Graphics {

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
    using Resource::Resource;
};
class IndexBuffer : public Resource {
public:
    using Resource::Resource;
};
struct MeshBuffer {
    std::unordered_map<std::string, std::shared_ptr<VertexBuffer>> vertices;
    std::shared_ptr<IndexBuffer>                                   indices;
    PrimitiveType                                                  primitive;
};
class ConstantBuffer : public Resource {
public:
    ConstantBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpu_resource, size_t num_element, size_t element_size)
        : Resource(name, std::move(gpu_resource)), m_NumElements(num_element), m_ElementSize(element_size) {}

    size_t GetNumElements() const { return m_NumElements; }
    size_t GetElementSize() const { return m_ElementSize; }

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
    } const desc;
    Sampler(std::string_view name, std::unique_ptr<backend::Resource> sampler, Description desc)
        : Resource(name, std::move(sampler)), desc(std::move(desc)) {}
};

}  // namespace Hitagi::Graphics