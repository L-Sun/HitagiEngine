#pragma once
#include "Format.hpp"
#include "SceneObject.hpp"

#include <string>
#include <memory>

namespace Hitagi::Graphics {

class Resource {
public:
    Resource(std::string_view name) : name(name) {}
    virtual ~Resource() = default;

protected:
    std::string name;
};

class ResourceContainer {
public:
    ResourceContainer() = default;
    ResourceContainer(std::unique_ptr<Resource>&& res) : m_Resource(std::move(res)) {}

    Resource*       GetResource() noexcept { return m_Resource.get(); }
    const Resource* GetResource() const noexcept { return m_Resource.get(); }

    operator bool() const noexcept { return m_Resource != nullptr; }

protected:
    std::unique_ptr<Resource> m_Resource;
};

class VertexBuffer : public ResourceContainer {
public:
    using ResourceContainer::ResourceContainer;
};
class IndexBuffer : public ResourceContainer {
public:
    using ResourceContainer::ResourceContainer;
};
struct MeshBuffer {
    std::unordered_map<std::string, VertexBuffer> vertices;
    IndexBuffer                                   indices;
    Asset::PrimitiveType                          primitive;
};
class ConstantBuffer : public ResourceContainer {
public:
    ConstantBuffer() = default;
    ConstantBuffer(std::unique_ptr<Resource>&& res, size_t numElement, size_t elementSize)
        : ResourceContainer(std::move(res)), m_NumElements(numElement), m_ElementSize(elementSize) {}

    size_t GetNumElements() const { return m_NumElements; }
    size_t GetElementSize() const { return m_ElementSize; }

private:
    size_t m_NumElements = 0;
    size_t m_ElementSize = 0;
};

class TextureBuffer : public ResourceContainer {
public:
    using ResourceContainer::ResourceContainer;
    struct Description {
        Format         format;
        uint32_t       width;
        uint32_t       height;
        uint32_t       pitch;
        unsigned       mipLevel;
        unsigned       sampleCount;
        unsigned       sampleQuality;
        const uint8_t* initialData     = nullptr;
        size_t         initialDataSize = 0;
    };
};

class DepthBuffer : public ResourceContainer {
public:
    using ResourceContainer::ResourceContainer;
    struct Description {
        Format   format;
        uint32_t width;
        uint32_t height;
        float    clearDepth;
        uint8_t  clearStencil;
    };
};

class RenderTarget : public ResourceContainer {
public:
    struct Description {
        Format   format;
        uint32_t width;
        uint32_t height;
    };
    using ResourceContainer::ResourceContainer;
};

}  // namespace Hitagi::Graphics