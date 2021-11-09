#pragma once
#include "Resource.hpp"
#include "RenderPass.hpp"
#include "DriverAPI.hpp"

#include <vector>
#include <functional>
#include <variant>
#include <unordered_set>

namespace Hitagi::Graphics {

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
    bool        sideEffect = false;
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
        FrameHandle Create(std::string_view name, typename T::Description desc) const noexcept { return fg.Create(name, desc); }
        FrameHandle Read(const FrameHandle input) const noexcept { return node.Read(input); }
        FrameHandle Write(const FrameHandle output) const noexcept { return node.Write(fg, output); }
        void        SideEffect() noexcept { node.sideEffect = true; }

    private:
        Builder(FrameGraph& fg, PassNode& node) : fg(fg), node(node) {}
        FrameGraph& fg;
        PassNode&   node;
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

    FrameHandle Import(RenderTarget* renderTarget) {
        FrameResourceId id     = m_ResourceCounter++;
        FrameHandle     handle = m_ResourceNodes.size();
        m_ResourceNodes.emplace_back(renderTarget->GetName(), id);
        m_ValidResources.emplace(id, renderTarget);
        return handle;
    }

    void Present(FrameHandle renderTarget, std::shared_ptr<Hitagi::Graphics::IGraphicsCommandContext> context);

    void Compile();
    void Execute(DriverAPI& driver);
    void Retire(uint64_t fenceValue, DriverAPI& driver) noexcept;

private:
    using Desc = std::variant<DepthBuffer::Description,
                              TextureBuffer::Description,
                              RenderTarget::Description>;

    FrameHandle Create(std::string_view name, Desc desc);

    bool m_Retired = false;

    std::vector<ResourceNode> m_ResourceNodes;
    std::vector<PassNode>     m_PassNodes;

    FrameResourceId                                m_ResourceCounter = 0;
    std::vector<Resource>                          m_InnerResources;
    std::unordered_map<FrameResourceId, Desc>      m_InnerResourcesDesc;
    std::unordered_map<FrameResourceId, Resource*> m_ValidResources;
};

class ResourceHelper {
    friend FrameGraph;

public:
    template <typename T>
    T& Get(FrameHandle handle) const {
        assert(node.reads.contains(handle) || node.writes.contains(handle) && "This pass node do not operate the handle in graph!");
        auto id = fg.m_ResourceNodes[handle].resource;
        return static_cast<T&>(*(fg.m_ValidResources.at(id)));
    }

private:
    ResourceHelper(FrameGraph& fg, PassNode& node) : fg(fg), node(node) {}
    FrameGraph& fg;
    PassNode&   node;
};

}  // namespace Hitagi::Graphics
