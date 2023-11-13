#pragma once
#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/concepts.hpp>
#include <hitagi/utils/private_build.hpp>

#include <string_view>
#include <unordered_set>

namespace hitagi::rg {

class RenderGraph;
class PassBuilder;

class RenderGraphNode;

class ResourceNode;
class GPUBufferNode;
class TextureNode;
class SamplerNode;
class RenderPipelineNode;
class ComputePipelineNode;

class PassNode;
class RenderPassNode;
class ComputePassNode;
class CopyPassNode;
class PresentPassNode;

class RenderGraphEdge;

class RenderGraphNode {
public:
    friend RenderGraph;
    friend PassBuilder;

    enum struct Type : std::uint8_t {
        GPUBuffer,
        Texture,
        Sampler,
        RenderPipeline,
        ComputePipeline,
        RenderPass,
        ComputePass,
        CopyPass,
        PresentPass,
    };

    RenderGraphNode(const RenderGraphNode&)            = delete;
    RenderGraphNode(RenderGraphNode&&) noexcept        = default;
    RenderGraphNode& operator=(const RenderGraphNode&) = delete;
    RenderGraphNode& operator=(RenderGraphNode&&)      = default;
    virtual ~RenderGraphNode()                         = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto GetType() const noexcept { return m_Type; }

    inline bool IsResourceNode() const noexcept {
        return m_Type == Type::GPUBuffer ||
               m_Type == Type::Texture ||
               m_Type == Type::Sampler ||
               m_Type == Type::RenderPipeline ||
               m_Type == Type::ComputePipeline;
    }
    inline bool IsPassNode() const noexcept { return m_Type == Type::RenderPass || m_Type == Type::ComputePass || m_Type == Type::CopyPass || m_Type == Type::PresentPass; }

    void AddInputNode(RenderGraphNode* node) noexcept;

protected:
    RenderGraphNode(RenderGraph& render_graph, Type type, std::string_view name = "")
        : m_RenderGraph(&render_graph), m_Type(type), m_Name(name) {}

    virtual void Initialize() = 0;

    RenderGraph* m_RenderGraph;
    Type         m_Type;

    std::pmr::unordered_set<RenderGraphNode*> m_InputNodes;
    std::pmr::unordered_set<RenderGraphNode*> m_OutputNodes;

    std::size_t      m_Handle;  // finish by RenderGraph or PassBuilder
    std::pmr::string m_Name;    // finish by PassBuilder when type is any of PassNode
};

inline constexpr auto gfx_resource_type_to_node_type(gfx::ResourceType type) {
    switch (type) {
        case gfx::ResourceType::GPUBuffer:
            return RenderGraphNode::Type::GPUBuffer;
        case gfx::ResourceType::Texture:
            return RenderGraphNode::Type::Texture;
        case gfx::ResourceType::Sampler:
            return RenderGraphNode::Type::Sampler;
        case gfx::ResourceType::RenderPipeline:
            return RenderGraphNode::Type::RenderPipeline;
        case gfx::ResourceType::ComputePipeline:
            return RenderGraphNode::Type::ComputePipeline;
        default:
            throw std::invalid_argument("Invalid resource type");
    }
}

struct GPUBufferEdge;
struct TextureEdge;

template <RenderGraphNode::Type T>
struct RenderGraphHandle {
    constexpr RenderGraphHandle(std::size_t id = std::numeric_limits<std::size_t>::max()) noexcept : index(id) {}
    inline constexpr std::strong_ordering operator<=>(const RenderGraphHandle&) const noexcept = default;
    inline constexpr bool                 operator==(const RenderGraphHandle&) const noexcept  = default;
    inline explicit constexpr             operator bool() const noexcept { return index != std::numeric_limits<std::size_t>::max(); }

    std::size_t           index;
    RenderGraphNode::Type type = T;
};

using GPUBufferHandle       = RenderGraphHandle<RenderGraphNode::Type::GPUBuffer>;
using TextureHandle         = RenderGraphHandle<RenderGraphNode::Type::Texture>;
using SamplerHandle         = RenderGraphHandle<RenderGraphNode::Type::Sampler>;
using RenderPipelineHandle  = RenderGraphHandle<RenderGraphNode::Type::RenderPipeline>;
using ComputePipelineHandle = RenderGraphHandle<RenderGraphNode::Type::ComputePipeline>;
using RenderPassHandle      = RenderGraphHandle<RenderGraphNode::Type::RenderPass>;
using ComputePassHandle     = RenderGraphHandle<RenderGraphNode::Type::ComputePass>;
using CopyPassHandle        = RenderGraphHandle<RenderGraphNode::Type::CopyPass>;
using PresentPassHandle     = RenderGraphHandle<RenderGraphNode::Type::PresentPass>;

}  // namespace hitagi::rg

namespace std {
template <hitagi::rg::RenderGraphNode::Type T>
struct hash<hitagi::rg::RenderGraphHandle<T>> {
    constexpr inline size_t operator()(const hitagi::rg::RenderGraphHandle<T>& handle) const noexcept {
        return hash<std::size_t>()(handle.index);
    }
};
}  // namespace std