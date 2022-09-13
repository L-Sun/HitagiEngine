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
    using ResourceType = std::variant<resource::Texture>;

    class Builder {
        friend class RenderGraph;

    public:
        ResourceHandle Create(std::string_view name, ResourceType res) const noexcept { return m_Fg.CreateResource(name, std::move(res)); }

        ResourceHandle Read(const ResourceHandle input) const noexcept { return m_Node.Read(input); }
        ResourceHandle Write(const ResourceHandle output) const noexcept { return m_Node.Write(m_Fg, output); }

    private:
        Builder(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode&    m_Node;
    };

    RenderGraph(DeviceAPI& device) : m_Device(device) {}

    ~RenderGraph() { assert(m_Retired && "Frame graph must set a fence to retire its resources."); }

    template <typename PassData>
    using SetupFunc = std::function<void(Builder&, PassData&)>;
    template <typename PassData>
    using ExecFunc = std::function<void(const ResourceHelper&, const PassData&, IGraphicsCommandContext* context)>;

    template <typename PassData>
    const PassData& AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData> executor);

    ResourceHandle Import(resource::Resource* res) {
        ResourceHandle handle = m_ResourceNodes.size();
        m_Resources.emplace_back(res);
        m_BlackBoard.emplace_back(res);
        m_ResourceNodes.emplace_back(res->name, res);
        return handle;
    }

    // if back buffer is null, make sure the tex is associated with swapchian
    void PresentPass(ResourceHandle texture, const std::shared_ptr<resource::Texture>& back_buffer = nullptr);

    bool Compile();
    void Execute();
    void Retire(std::uint64_t fence_value);

private:
    ResourceHandle CreateResource(std::string_view name, ResourceType resource);

    DeviceAPI&   m_Device;
    bool         m_Retired = false;
    tf::Taskflow m_Taskflow;

    std::pmr::vector<ResourceNode> m_ResourceNodes;
    std::pmr::vector<PassNode>     m_PassNodes;
    PassNode                       m_PresentPassNode;

    std::pmr::vector<resource::Resource*> m_Resources;  // All the resource including inner and black board
    std::pmr::vector<ResourceType>        m_InnerResources;
    std::pmr::vector<resource::Resource*> m_BlackBoard;
};

class ResourceHelper {
    friend RenderGraph;

public:
    template <typename T>
    T& Get(ResourceHandle handle) const {
        assert((m_Node.reads.contains(handle) || m_Node.writes.contains(handle)) && "This pass node do not operate the handle in graph!");
        auto result = m_Fg.m_ResourceNodes[handle].resource;
        assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
        return *std::static_pointer_cast<T>(result);
    }

private:
    ResourceHelper(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
    RenderGraph& m_Fg;
    PassNode&    m_Node;
};

template <typename PassData>
const PassData& RenderGraph::AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData> executor) {
    // Transfer ownership to pass node
    PassNode& node = m_PassNodes.emplace_back(PassNode{
        .name = std::pmr::string(name),
        .data = core::Buffer(sizeof(PassData)),
    });
    node.executor  = [exec = std::move(executor), &node, this](IGraphicsCommandContext* context) {
        exec(ResourceHelper(*this, node), node.GetData<PassData>(), context);
    };

    Builder builder(*this, node);
    setup(builder, node.GetData<PassData>());
    return node.GetData<PassData>();
}

}  // namespace hitagi::gfx
