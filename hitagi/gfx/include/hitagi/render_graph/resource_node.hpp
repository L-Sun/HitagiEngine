#pragma once
#include <hitagi/render_graph/common_types.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

namespace hitagi::rg {

class ResourceNode : public RenderGraphNode {
public:
    friend RenderGraph;

    auto GetWriter() const noexcept -> PassNode*;

protected:
    ResourceNode(RenderGraph& render_graph, Type type, std::string_view name = "", std::shared_ptr<gfx::Resource> resource = nullptr);

    bool                           m_IsImported = false;
    std::shared_ptr<gfx::Resource> m_Resource;
};

class GPUBufferNode : public ResourceNode {
public:
    friend RenderGraph;

    GPUBufferNode(RenderGraph& render_graph, gfx::GPUBufferDesc desc, std::string_view name = "");
    GPUBufferNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> buffer, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::GPUBuffer&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::GPUBufferDesc&;

    auto        Move(GPUBufferHandle new_handle, std::string_view new_name) -> std::shared_ptr<GPUBufferNode>;
    inline auto GetMoveNode() const noexcept { return m_MoveToNode; }
    inline auto GetMoveFromNode() const noexcept { return m_MoveFromNode; }

protected:
    void Initialize() final;

    std::optional<gfx::GPUBufferDesc> m_Desc;
    GPUBufferNode*                    m_MoveToNode   = nullptr;
    GPUBufferNode*                    m_MoveFromNode = nullptr;
};

class TextureNode : public ResourceNode {
public:
    friend RenderGraph;

    TextureNode(RenderGraph& render_graph, gfx::TextureDesc desc, std::string_view name = "");
    TextureNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> texture, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::Texture&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::TextureDesc&;

    auto        Move(TextureHandle new_handle, std::string_view new_name) -> std::shared_ptr<TextureNode>;
    inline auto GetMoveNode() const noexcept { return m_MoveToNode; }
    inline auto GetMoveFromNode() const noexcept { return m_MoveFromNode; }

protected:
    void Initialize() final;

    std::optional<gfx::TextureDesc> m_Desc;
    TextureNode*                    m_MoveToNode   = nullptr;
    TextureNode*                    m_MoveFromNode = nullptr;
};

class SamplerNode : public ResourceNode {
public:
    friend RenderGraph;

    SamplerNode(RenderGraph& render_graph, gfx::SamplerDesc desc, std::string_view name = "");
    SamplerNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> sampler, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::Sampler&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::SamplerDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::SamplerDesc> m_Desc;
};

class RenderPipelineNode : public ResourceNode {
public:
    friend RenderGraph;

    RenderPipelineNode(RenderGraph& render_graph, gfx::RenderPipelineDesc desc, std::string_view name = "");
    RenderPipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::RenderPipeline&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::RenderPipelineDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::RenderPipelineDesc> m_Desc;
};

class ComputePipelineNode : public ResourceNode {
public:
    friend RenderGraph;

    ComputePipelineNode(RenderGraph& render_graph, gfx::ComputePipelineDesc desc, std::string_view name = "");
    ComputePipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::ComputePipeline&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::ComputePipelineDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::ComputePipelineDesc> m_Desc;
};

}  // namespace hitagi::rg