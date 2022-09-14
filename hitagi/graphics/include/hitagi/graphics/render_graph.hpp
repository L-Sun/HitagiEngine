#pragma once
#include <hitagi/graphics/device_api.hpp>
#include <hitagi/graphics/pass_node.hpp>
#include <hitagi/graphics/resource_node.hpp>

#include <taskflow/taskflow.hpp>

#include <memory>
#include <set>

namespace hitagi::gfx {

class RenderGraph {
    friend class ResourceHelper;
    friend struct PassNode;

public:
    using ResourceType = std::variant<resource::Texture, gfx::ConstantBuffer>;

    class Builder {
        friend class RenderGraph;

    public:
        ResourceHandle Create(std::string_view name, ResourceType res) const noexcept {
            return m_Node.Write(m_Fg, m_Fg.CreateResource(name, std::move(res)));
        }

        ResourceHandle Read(const ResourceHandle input) const noexcept { return m_Node.Read(input); }
        ResourceHandle Write(const ResourceHandle output) const noexcept { return m_Node.Write(m_Fg, output); }

    private:
        Builder(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode&    m_Node;
    };

    class ResourceHelper {
        friend RenderGraph;

    public:
        template <typename T>
        T& Get(ResourceHandle handle) const {
            assert((m_Node.reads.contains(handle) || m_Node.writes.contains(handle)) && "This pass node do not operate the handle in graph!");
            auto result = m_Fg.m_ResourceNodes[handle].resource;
            assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
            return *static_cast<T*>(result);
        }

    private:
        ResourceHelper(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode&    m_Node;
    };

    RenderGraph(DeviceAPI& device) : m_Device(device) {}

    ~RenderGraph() { Reset(); }

    template <typename PassData>
    using SetupFunc = std::function<void(Builder&, PassData&)>;
    template <typename PassData>
    using ExecFunc = std::function<void(ResourceHelper&, const PassData&, IGraphicsCommandContext* context)>;

    template <typename PassData>
    const PassData& AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData> executor);

    ResourceHandle Import(resource::Resource* res);

    void PresentPass(ResourceHandle texture, resource::Texture* back_buffer = nullptr);

    bool Compile();
    void Execute();
    void Reset();

private:
    ResourceHandle CreateResource(std::string_view name, ResourceType resource);

    DeviceAPI&    m_Device;
    std::uint64_t m_LastFence = 0;

    tf::Taskflow m_Taskflow;
    tf::Executor m_Executor;

    std::pmr::vector<ResourceNode> m_ResourceNodes;
    std::pmr::vector<PassNode>     m_PassNodes;
    PassNode                       m_PresentPassNode;

    std::pmr::vector<resource::Resource*> m_Resources;  // All the resource including inner and black board
    std::pmr::vector<ResourceType>        m_InnerResources;
    std::pmr::vector<resource::Resource*> m_BlackBoard;
};

template <typename PassData>
const PassData& RenderGraph::AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData> executor) {
    // Transfer ownership to pass node
    PassNode& node = m_PassNodes.emplace_back(PassNode{
        .name = std::pmr::string(name),
        .data = core::Buffer(sizeof(PassData)),
    });
    node.executor  = [exec = std::move(executor), &node, this](IGraphicsCommandContext* context) {
        ResourceHelper helper(*this, node);
        exec(helper, node.GetData<PassData>(), context);
    };

    Builder builder(*this, node);
    setup(builder, node.GetData<PassData>());
    return node.GetData<PassData>();
}

}  // namespace hitagi::gfx
