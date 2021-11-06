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
    std::vector<FrameHandle> reads;
    std::vector<FrameHandle> writes;

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

    FrameHandle Import(RenderTarget& renderTarget) {
        FrameHandle handle = m_ResourceNodes.size();
        m_ResourceNodes.emplace_back(renderTarget.GetResource()->GetName(), m_Resources.size());
        m_Resources.emplace_back(renderTarget);
        return handle;
    }

    void Present(FrameHandle renderTarget) {
    }

    void Compile();
    void Execute(backend::DriverAPI& driver);
    void Retire(uint64_t fenceValue, backend::DriverAPI& driver) noexcept;

private:
    using Desc = std::variant<DepthBuffer::Description,
                              TextureBuffer::Description,
                              RenderTarget::Description>;

    FrameHandle Create(std::string_view name, Desc desc);

    bool m_Retired = false;

    std::vector<ResourceNode> m_ResourceNodes;
    std::vector<PassNode>     m_PassNodes;

    std::vector<ResourceContainer> m_Resources;
    // m_ResourcesDesc will create resource for m_Resources. here the key is the index of m_Resources
    std::unordered_map<size_t, Desc> m_ResourcesDesc;
    // resources container index
    std::unordered_set<FrameResourceId> m_ValidResource;
};

class ResourceHelper {
    friend FrameGraph;

public:
    template <typename T>
    T& Get(FrameHandle handle) const {
        auto id = fg.m_ResourceNodes[handle].resource;
        assert(fg.m_ValidResource.contains(id) && "can not access invalid resource after complie!");
        return static_cast<T&>(fg.m_Resources.at(id));
    }

private:
    ResourceHelper(FrameGraph& fg, PassNode& node) : fg(fg), node(node) {}
    FrameGraph& fg;
    PassNode&   node;
};

}  // namespace Hitagi::Graphics
