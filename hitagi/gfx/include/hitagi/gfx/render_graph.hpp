#pragma once
#include <hitagi/gfx/device.hpp>
#include <hitagi/gfx/pass_node.hpp>
#include <hitagi/gfx/resource_node.hpp>

#include <taskflow/taskflow.hpp>

#include <memory>
#include <set>

namespace hitagi::gfx {

class RenderGraph {
    friend class ResourceHelper;
    friend struct PassNode;

public:
    using ResourceDesc = std::variant<GpuBuffer::Desc, Texture::Desc>;

    class Builder {
        friend class RenderGraph;

    public:
        ResourceHandle Create(ResourceDesc desc) const noexcept {
            return m_Node->Write(m_Fg, m_Fg.CreateResource(desc));
        }

        ResourceHandle Read(const ResourceHandle input) const noexcept { return m_Node->Read(input); }
        ResourceHandle Write(const ResourceHandle output) const noexcept { return m_Node->Write(m_Fg, output); }

    private:
        Builder(RenderGraph& fg, PassNode* node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode*    m_Node;
    };

    class ResourceHelper {
        friend RenderGraph;

    public:
        template <typename T>
        std::shared_ptr<T> Get(ResourceHandle handle) const {
            if (!(m_Node->reads.contains(handle) || m_Node->writes.contains(handle))) {
                throw std::logic_error(fmt::format("This pass node do not operate the handle{} in graph", handle));
            }
            auto result = m_Fg.m_Resources[m_Fg.m_ResourceNodes[handle].res_idx];
            assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
            return std::static_pointer_cast<T>(result);
        }

    private:
        ResourceHelper(RenderGraph& fg, PassNode* node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode*    m_Node;
    };

    RenderGraph(Device& device) : m_Device(device) {}

    template <typename PassData>
    using SetupFunc = std::function<void(Builder&, PassData&)>;

    template <typename PassData, typename Context>
    using ExecFunc = std::function<void(const ResourceHelper&, const PassData&, Context* context)>;

    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, GraphicsCommandContext> executor) -> const PassData&;
    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, ComputeCommandContext> executor) -> const PassData&;
    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, CopyCommandContext> executor) -> const PassData&;

    auto Import(std::string_view name, std::shared_ptr<Resource> res) -> ResourceHandle;

    void PresentPass(ResourceHandle color, ResourceHandle back_buffer = -1);

    bool Compile();
    void Execute();
    void Reset();

private:
    template <typename PassData, typename Context>
    auto AddPassImpl(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, Context> executor) -> const PassData&;
    auto CreateResource(ResourceDesc desc) -> ResourceHandle;
    auto RequestCommandContext(CommandType type) -> std::shared_ptr<CommandContext>;

    Device&       m_Device;
    std::uint64_t m_LastFence = 0;

    tf::Executor m_Executor;
    tf::Taskflow m_Taskflow;

    std::pmr::vector<ResourceNode>              m_ResourceNodes;
    std::pmr::vector<std::shared_ptr<PassNode>> m_PassNodes;
    std::shared_ptr<PassNode>                   m_PresentPassNode = nullptr;

    std::pmr::vector<std::shared_ptr<Resource>> m_Resources;
    // dict<name, index>, where the `index` points to m_Resources
    std::pmr::unordered_map<std::pmr::string, std::size_t> m_BlackBoard;
    // dict<desc, index>, where the `index` points to m_Resources
    std::pmr::vector<std::pair<ResourceDesc, std::size_t>> m_InnerResourcesDesc;

    std::pmr::vector<PassNode*> m_ExecuteQueue;

    utils::EnumArray<std::pmr::list<std::shared_ptr<CommandContext>>, CommandType> m_ContextPool;
};

template <typename PassData>
auto RenderGraph::AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, GraphicsCommandContext> executor) -> const PassData& {
    return AddPassImpl(name, setup, executor);
}
template <typename PassData>
auto RenderGraph::AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, ComputeCommandContext> executor) -> const PassData& {
    return AddPassImpl(name, setup, executor);
}
template <typename PassData>
auto RenderGraph::AddPass(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, CopyCommandContext> executor) -> const PassData& {
    return AddPassImpl(name, setup, executor);
}

template <typename PassData, typename Context>
auto RenderGraph::AddPassImpl(std::string_view name, SetupFunc<PassData> setup, ExecFunc<PassData, Context> executor) -> const PassData& {
    auto node = std::make_shared<PassNodeWithData<Context, PassData>>();
    m_PassNodes.emplace_back(node);

    node->name = name;
    if constexpr (std::is_same_v<Context, GraphicsCommandContext>) {
        node->type = CommandType::Graphics;
    }
    if constexpr (std::is_same_v<Context, ComputeCommandContext>) {
        node->type = CommandType::Compute;
    }
    if constexpr (std::is_same_v<Context, CopyCommandContext>) {
        node->type = CommandType::Copy;
    }

    // _node is to void shared_ptr cycle dependency
    node->executor = [exec = std::move(executor), _node = node.get(), this](Context* context) {
        exec(ResourceHelper(*this, _node), _node->data, context);
    };

    Builder builder(*this, node.get());
    setup(builder, node->data);
    return node->data;
}

}  // namespace hitagi::gfx
