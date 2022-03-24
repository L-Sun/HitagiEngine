#pragma once
#include "Resource.hpp"
#include "RenderPass.hpp"
#include "DriverAPI.hpp"

#include <vector>
#include <functional>
#include <variant>
#include <unordered_set>

namespace hitagi::graphics {

using FrameHandle     = size_t;
using FrameResourceId = size_t;
struct PassNode;
class FrameGraph;

struct ResourceNode {
    ResourceNode(std::string_view name, FrameResourceId resource, PassNode* writer = nullptr, unsigned version = 0)
        : name(std::string(name)), resource(resource), writer(writer), version(version) {}
    std::string     name;
    FrameResourceId resource;
    PassNode*       writer;
    unsigned        version;
};
struct PassNode {
    PassNode(std::string_view name, PassExecutor* executor) : name(name), executor(executor) {}

    FrameHandle Read(const FrameHandle input);
    FrameHandle Write(FrameGraph& fg, const FrameHandle output);

    std::string name;
    bool        side_effect = false;
    // index of the resource node in frame graph
    std::set<FrameHandle> reads;
    std::set<FrameHandle> writes;

    std::unique_ptr<PassExecutor> executor;
};

class FrameGraph {
    friend PassNode;
    friend class ResourceHelper;

public:
    class Builder {
        friend class FrameGraph;

    public:
        template <typename T>
        FrameHandle Create(std::string_view name, typename T::Description desc) const noexcept { return m_Fg.Create(name, desc); }
        FrameHandle Read(const FrameHandle input) const noexcept { return m_Node.Read(input); }
        FrameHandle Write(const FrameHandle output) const noexcept { return m_Node.Write(m_Fg, output); }
        void        SideEffect() noexcept { m_Node.side_effect = true; }

    private:
        Builder(FrameGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
        FrameGraph& m_Fg;
        PassNode&   m_Node;
    };

    FrameGraph() = default;
    ~FrameGraph() { assert(m_Retired && "Frame graph must set a fence to retire its resources."); }

    template <typename PassData, typename SetupFunc, typename ExecuteFunc>
    RenderPass<PassData, ExecuteFunc> AddPass(std::string_view name, SetupFunc&& setup, ExecuteFunc&& execute) {
        auto pass = new RenderPass<PassData, ExecuteFunc>(std::forward<ExecuteFunc>(execute));
        // Transfer ownership to pass node
        PassNode& node = m_PassNodes.emplace_back(name, pass);
        Builder   builder(*this, node);
        setup(builder, pass->GetData());
        return *pass;
    }

    FrameHandle Import(std::shared_ptr<RenderTarget> render_target) {
        FrameResourceId id     = m_Resources.size();
        FrameHandle     handle = m_ResourceNodes.size();
        m_Resources.emplace_back(render_target);
        m_ResourceNodes.emplace_back(render_target->GetName(), id);
        return handle;
    }

    void Present(FrameHandle render_target, std::shared_ptr<hitagi::graphics::IGraphicsCommandContext> context);

    void Compile();
    void Execute(DriverAPI& driver);
    void Retire(uint64_t fence_value, DriverAPI& driver) noexcept;

private:
    using Desc = std::variant<DepthBuffer::Description,
                              TextureBuffer::Description,
                              RenderTarget::Description>;

    FrameHandle Create(std::string_view name, Desc desc);

    bool m_Retired = false;

    std::vector<ResourceNode> m_ResourceNodes;
    std::vector<PassNode>     m_PassNodes;

    std::vector<std::shared_ptr<Resource>>    m_Resources;
    std::unordered_map<FrameResourceId, Desc> m_InnerResourcesDesc;
};

class ResourceHelper {
    friend FrameGraph;

public:
    template <typename T>
    T& Get(FrameHandle handle) const {
        assert((m_Node.reads.contains(handle) || m_Node.writes.contains(handle)) && "This pass node do not operate the handle in graph!");
        auto id     = m_Fg.m_ResourceNodes[handle].resource;
        auto result = m_Fg.m_Resources[id];
        assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
        return static_cast<T&>(*result);
    }

private:
    ResourceHelper(FrameGraph& fg, PassNode& node) : m_Fg(fg), m_Node(node) {}
    FrameGraph& m_Fg;
    PassNode&   m_Node;
};

}  // namespace hitagi::graphics
