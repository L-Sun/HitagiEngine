#pragma once
#include "Resource.hpp"
#include "FramePass.hpp"
#include "DriverAPI.hpp"

#include <vector>
#include <functional>
#include <variant>

namespace Hitagi::Graphics {

using FrameHandle     = size_t;
using FrameResourceId = size_t;
struct PassNode;
class FrameGraph;

struct ResourceNode {
    ResourceNode(FrameResourceId resource, PassNode* writer = nullptr, unsigned version = 0)
        : resource(resource), writer(writer), version(version) {}
    FrameResourceId resource;
    PassNode*       writer;
    unsigned        version;
};
struct PassNode {
    PassNode(std::string_view name, PassExecutor* pass) : name(name), pass(pass) {}

    FrameHandle Read(FrameHandle input);
    FrameHandle Write(FrameGraph& fg, FrameHandle output);

    std::string name;
    // index of the resource node in frame graph
    std::vector<FrameHandle> reads;
    std::vector<FrameHandle> writes;

    std::unique_ptr<PassExecutor> pass;
};

class FrameGraph {
    friend PassNode;
    friend class ResourceHelper;

public:
    class Builder {
        friend class FrameGraph;

    public:
        template <typename T>
        FrameHandle Create(typename T::Description desc) const noexcept { return fg.Create(desc); }
        FrameHandle Read(FrameHandle input) const noexcept { return node.Read(input); }
        FrameHandle Write(FrameHandle output) const noexcept { return node.Write(fg, output); }

    private:
        Builder(FrameGraph& fg, PassNode& node) : fg(fg), node(node) {}
        FrameGraph& fg;
        PassNode&   node;
    };

    FrameGraph(backend::DriverAPI& driver) : m_Driver(driver) {}
    ~FrameGraph() { assert(m_Retired && "Frame graph must set a fence to retire its resources."); }

    template <typename PassData, typename SetupFunc, typename ExecuteFunc>
    FramePass<PassData, ExecuteFunc> AddPass(std::string_view name, SetupFunc&& setup, ExecuteFunc&& execute) {
        auto pass = new FramePass<PassData, ExecuteFunc>(std::forward<ExecuteFunc>(execute));
        // Transfer ownership to pass node
        PassNode& node = m_PassNodes.emplace_back(name, pass);
        Builder   builder(*this, node);
        setup(builder, pass->GetData());
        return *pass;
    }

    void Compile();
    void Execute();
    void Retire(uint64_t fenceValue) noexcept;

private:
    using Desc = std::variant<DepthBuffer::Description,
                              TextureBuffer::Description,
                              RenderTarget::Description>;

    FrameHandle Create(Desc desc);

    backend::DriverAPI& m_Driver;
    bool                m_Retired = false;

    std::vector<ResourceNode> m_ResourceNodes;
    std::vector<PassNode>     m_PassNodes;

    std::vector<ResourceContainer> m_Resources;
    std::vector<Desc>              m_ResourcesDesc;
    // resources container index
    std::unordered_map<FrameResourceId, size_t> m_ValidResource;
};

class ResourceHelper {
    friend FrameGraph;

public:
    template <typename T>
    T& Get(FrameHandle handle) const {
        using Desc = typename T::Description;
        auto id    = fg.m_ResourceNodes[handle].resource;
        assert(std::holds_alternative<Desc>(fg.m_ResourcesDesc[id]));
        return static_cast<T&>(fg.m_Resources[fg.m_ValidResource.at(id)]);
    }

private:
    ResourceHelper(FrameGraph& fg, PassNode& node) : fg(fg), node(node) {}
    FrameGraph& fg;
    PassNode&   node;
};

}  // namespace Hitagi::Graphics
