module;
#include <spdlog/spdlog.h>

export module gfx.render_graph:resource_node;
import std;
import utils;
import gfx.base;
import :type;
import :resource_edge;

export namespace hitagi::rg {

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
    inline auto  GetBuffer() const noexcept -> std::shared_ptr<gfx::GPUBuffer> { return std::static_pointer_cast<gfx::GPUBuffer>(m_Resource); }
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
    inline auto  GetTexture() const noexcept -> std::shared_ptr<gfx::Texture> { return std::static_pointer_cast<gfx::Texture>(m_Resource); }
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

}  // namespace hitagi::rg

namespace hitagi::rg {

ResourceNode::ResourceNode(RenderGraph& render_graph, Type type, std::string_view name, std::shared_ptr<gfx::Resource> resource)
    : RenderGraphNode(render_graph, type, name), m_IsImported(resource != nullptr), m_Resource(std::move(resource)) {
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, gfx::GPUBufferDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::GPUBuffer, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

GPUBufferNode::GPUBufferNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> buffer, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::GPUBuffer, name, std::move(buffer)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto GPUBufferNode::GetDesc() const noexcept -> const gfx::GPUBufferDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::GPUBuffer>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

TextureNode::TextureNode(RenderGraph& render_graph, gfx::TextureDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Texture, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = m_Desc->name;
}

TextureNode::TextureNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> texture, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Texture, name, std::move(texture)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto TextureNode::GetDesc() const noexcept -> const gfx::TextureDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::Texture>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

SamplerNode::SamplerNode(RenderGraph& render_graph, gfx::SamplerDesc desc, std::string_view name)
    : ResourceNode(render_graph, Type::Sampler, name), m_Desc(std::move(desc)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

SamplerNode::SamplerNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> sampler, std::string_view name)
    : ResourceNode(render_graph, RenderGraphNode::Type::Sampler, name, std::move(sampler)) {
    if (m_Name.empty()) m_Name = GetDesc().name;
}

auto SamplerNode::GetDesc() const noexcept -> const gfx::SamplerDesc& {
    if (m_Resource)
        return std::static_pointer_cast<gfx::Sampler>(m_Resource)->GetDesc();
    else
        return m_Desc.value();
}

}  // namespace hitagi::rg
