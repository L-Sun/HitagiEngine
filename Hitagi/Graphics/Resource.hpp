#pragma once
#include "Format.hpp"
#include "SceneObject.hpp"

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
    ConstantBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpuResource, size_t numElement, size_t elementSize)
        : Resource(name, std::move(gpuResource)), m_NumElements(numElement), m_ElementSize(elementSize) {}

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
        unsigned       mipLevel        = 1;
        unsigned       sampleCount     = 1;
        unsigned       sampleQuality   = 0;
        const uint8_t* initialData     = nullptr;
        size_t         initialDataSize = 0;
    } const desc;

    TextureBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpuResource, Description desc)
        : Resource(name, std::move(gpuResource)), desc(desc) {}
};

class DepthBuffer : public Resource {
public:
    struct Description {
        Format   format;
        uint64_t width;
        uint64_t height;
        float    clearDepth;
        uint8_t  clearStencil;
    } const desc;
    DepthBuffer(std::string_view name, std::unique_ptr<backend::Resource> gpuResource, Description desc)
        : Resource(name, std::move(gpuResource)), desc(desc) {}
};

class RenderTarget : public Resource {
public:
    struct Description {
        Format   format;
        uint64_t width;
        uint64_t height;
    } const desc;
    RenderTarget(std::string_view name, std::unique_ptr<backend::Resource> gpuResource, Description desc)
        : Resource(name, std::move(gpuResource)), desc(desc) {}
};

class Sampler : public Resource {
public:
    struct Description {
    } const desc;
    Sampler(std::string_view name, std::unique_ptr<backend::Resource> sampler, Description desc)
        : Resource(name, std::move(sampler)), desc(std::move(desc)) {}
};

}  // namespace Hitagi::Graphics