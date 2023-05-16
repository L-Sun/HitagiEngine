#pragma once
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/resource_node.hpp>

#include <variant>

namespace hitagi::gfx {
class RenderGraph;

using ResourceDesc = std::variant<GPUBufferDesc, TextureDesc, SamplerDesc>;

class PassBuilderBase {
public:
    PassBuilderBase(RenderGraph& rg, PassNodeBase& pass) : m_RenderGraph(rg), m_PassNode(pass) {}

private:
    RenderGraph&  m_RenderGraph;
    PassNodeBase& m_PassNode;
};

class RenderPassBuilder : public PassBuilderBase {
public:
    using PassBuilderBase::PassBuilderBase;

    auto CreateBuffer(GPUBufferDesc desc) noexcept -> ResourceHandle;
    auto CreateTexture(TextureDesc desc) noexcept -> ResourceHandle;
    auto CreateSampler(SamplerDesc desc) noexcept -> ResourceHandle;

    void ReadBuffer(ResourceHandle handle, std::size_t element_index) noexcept;
    void ReadBuffer(ResourceHandle handle, std::size_t offset, std::size_t size) noexcept;
    void ReadTexture(ResourceHandle handle, std::size_t mip_level = 0, std::size_t array_index = 0) noexcept;

    auto WriteBuffer(ResourceHandle handle, std::size_t element_index) noexcept -> ResourceHandle;
    auto WriteBuffer(ResourceHandle handle, std::size_t offset, std::size_t size) noexcept -> ResourceHandle;
    auto WriteTexture(ResourceHandle handle, std::size_t mip_level = 0, std::size_t array_index = 0) noexcept -> ResourceHandle;
};

class ComputePassBuilder : public PassBuilderBase {
public:
    using PassBuilderBase::PassBuilderBase;
};
class CopyPassBuilder : public PassBuilderBase {
public:
    using PassBuilderBase::PassBuilderBase;
};

}  // namespace hitagi::gfx