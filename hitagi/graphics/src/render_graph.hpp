#pragma once
#include "render_pass.hpp"

#include <hitagi/graphics/device_api.hpp>

#include <set>

namespace hitagi::graphics {

using FrameHandle     = std::size_t;
using FrameResourceId = std::size_t;
struct PassNode;
class RenderGraph;

struct ResourceNode {
    ResourceNode(std::string_view name, FrameResourceId resource)
        : name(name), resource(resource) {}

    std::pmr::string name;
    FrameResourceId  resource;
    PassNode*        writer  = nullptr;
    unsigned         version = 0;
};
struct PassNode {
    PassNode(std::string_view name, PassExecutor* executor)
        : name(name), executor(executor) {}

    FrameHandle Read(const FrameHandle input);
    FrameHandle Write(RenderGraph& fg, const FrameHandle output);

    std::pmr::string name;
    bool             side_effect = false;
    // index of the resource node in frame graph
    std::pmr::set<FrameHandle> reads;
    std::pmr::set<FrameHandle> writes;

    std::unique_ptr<PassExecutor> executor;
};

class RenderGraph {
    friend PassNode;
    friend class ResourceHelper;

public:
    using ResourceType = std::variant<DepthBuffer, resource::Texture, RenderTarget>;

    class Builder {
        friend class RenderGraph;

    public:
        FrameHandle Create(std::string_view name, ResourceType res) const noexcept { return m_Fg.Create(name, std::move(res)); }

        FrameHandle Read(const FrameHandle input) const noexcept { return m_Node.Read(input); }
        FrameHandle Write(const FrameHandle output) const noexcept { return m_Node.Write(m_Fg, output); }
        void        SideEffect() noexcept { m_Node.side_effect = true; }

    private:
        Builder(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode&    m_Node;
    };

    RenderGraph(DeviceAPI& device) : m_Device(device) {}

    ~RenderGraph() { assert(m_Retired && "Frame graph must set a fence to retire its resources."); }

    template <typename PassData, typename SetupFunc, typename ExecuteFunc>
    RenderPass<PassData, ExecuteFunc> AddPass(std::string_view name, SetupFunc&& setup, ExecuteFunc&& execute) {
        auto pass = new RenderPass<PassData, ExecuteFunc>(std::forward<ExecuteFunc>(execute));
        // Transfer ownership to pass node
        PassNode& node = m_PassNodes.emplace_back(name, pass);
        Builder   builder(*this, node);
        setup(builder, pass->GetData());
        return *pass;
    }

    FrameHandle Import(RenderTarget* render_target) {
        FrameResourceId id     = m_Resources.size();
        FrameHandle     handle = m_ResourceNodes.size();
        m_Resources.emplace_back(render_target);
        m_BlackBoard.emplace_back(render_target);
        m_ResourceNodes.emplace_back(render_target->name, id);
        return handle;
    }

    void Present(FrameHandle render_target, const std::shared_ptr<hitagi::graphics::IGraphicsCommandContext>& contex);

    void Compile();
    void Execute();
    void SetFenceValue(std::uint64_t fence_value);

private:
    FrameHandle Create(std::string_view name, ResourceType resource);

    DeviceAPI& m_Device;
    bool       m_Retired = false;

    std::pmr::vector<ResourceNode> m_ResourceNodes;
    std::pmr::vector<PassNode>     m_PassNodes;

    std::pmr::vector<resource::Resource*> m_Resources;  // All the resource including inner and black board
    std::pmr::vector<ResourceType>        m_InnerResources;
    std::pmr::vector<resource::Resource*> m_BlackBoard;
};

class ResourceHelper {
    friend RenderGraph;

public:
    template <typename T>
    T& Get(FrameHandle handle) const {
        assert((m_Node.reads.contains(handle) || m_Node.writes.contains(handle)) && "This pass node do not operate the handle in graph!");
        auto id     = m_Fg.m_ResourceNodes[handle].resource;
        auto result = m_Fg.m_Resources[id];
        assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
        return *static_cast<T*>(result);
    }

private:
    ResourceHelper(RenderGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
    RenderGraph& m_Fg;
    PassNode&    m_Node;
};

}  // namespace hitagi::graphics
