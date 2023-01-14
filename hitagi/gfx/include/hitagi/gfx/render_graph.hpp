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
        Builder(const Builder&) = delete;
        Builder(Builder&&)      = delete;

        auto Create(ResourceDesc desc) const -> ResourceHandle;
        auto Read(ResourceHandle input) const -> ResourceHandle;
        auto Write(ResourceHandle output) const -> ResourceHandle;
        void UseRenderPipeline(std::shared_ptr<RenderPipeline> pipeline) const;
        void UseComputePipeline(std::shared_ptr<ComputePipeline> pipeline) const;

    private:
        Builder(RenderGraph& render_graph, PassNode* node) : m_RenderGraph(render_graph), m_Node(node) {}
        RenderGraph& m_RenderGraph;
        PassNode*    m_Node;
    };

    class ResourceHelper {
        friend RenderGraph;

    public:
        template <typename T>
        T& Get(ResourceHandle handle) const {
            if (!(m_Node->reads.contains(handle) || m_Node->writes.contains(handle))) {
                throw std::logic_error(fmt::format("This pass node do not operate the handle{} in graph", handle.id));
            }
            auto result = m_Fg.m_Resources[m_Fg.m_ResourceNodes[handle.id - 1].res_idx];
            assert(result != nullptr && "Access a invalid resource in excution phase, which may be pruned in compile phase!");
            return *static_cast<T*>(result);
        }

    private:
        ResourceHelper(RenderGraph& fg, PassNode* node) : m_Fg(fg), m_Node(node) {}
        RenderGraph& m_Fg;
        PassNode*    m_Node;
    };

    RenderGraph(Device& device);

    template <typename PassData>
    using SetupFunc = std::function<void(Builder&, PassData&)>;

    template <typename PassData, typename Context>
    using ExecFunc = std::function<void(const ResourceHelper&, const PassData&, Context* context)>;

    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData&> setup, ExecFunc<const PassData&, GraphicsCommandContext> executor) -> const PassData&;
    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData&> setup, ExecFunc<const PassData&, ComputeCommandContext> executor) -> const PassData&;
    template <typename PassData>
    auto AddPass(std::string_view name, SetupFunc<PassData&> setup, ExecFunc<const PassData&, CopyCommandContext> executor) -> const PassData&;

    // render graph will keep the resource life until the command context finish its execution
    auto Import(std::string_view name, std::shared_ptr<Resource>) -> ResourceHandle;
    // render graph don not keep the resource life
    auto ImportWithoutLifeTrack(std::string_view name, Resource* res) -> ResourceHandle;
    // get the newest resource handle from back board
    auto GetImportedResourceHandle(std::string_view name) -> ResourceHandle;

    void PresentPass(ResourceHandle back_buffer);

    bool Compile();
    auto Execute() -> utils::EnumArray<std::uint64_t, CommandType>;
    void Reset();

    Device& device;

private:
    struct InnerResource {
        ResourceDesc              desc;
        std::shared_ptr<Resource> resource       = nullptr;
        std::size_t               resource_index = 0;  //`index` points to m_Resources
    };

    template <typename PassData, typename Context>
    auto AddPassImpl(std::string_view name, SetupFunc<PassData&> setup, ExecFunc<const PassData&, Context> executor) -> const PassData&;
    auto CreateResource(ResourceDesc desc) -> ResourceHandle;
    auto RequestCommandContext(CommandType type) -> std::shared_ptr<CommandContext>;
    auto GetResrouceNode(ResourceHandle handle) -> ResourceNode&;
    auto GetLifeTrackResource(const Resource* res) -> std::shared_ptr<Resource>;

    tf::Executor m_Executor;
    tf::Taskflow m_Taskflow;

    std::pmr::vector<ResourceNode>              m_ResourceNodes;
    std::pmr::vector<std::shared_ptr<PassNode>> m_PassNodes;
    std::shared_ptr<PassNode>                   m_PresentPassNode = nullptr;

    std::pmr::vector<Resource*> m_Resources;
    // dict<name, index>, where the `index` points to m_Resources
    std::pmr::unordered_map<std::pmr::string, std::size_t> m_BlackBoard;
    std::pmr::vector<std::shared_ptr<Resource>>            m_OutterResources;
    std::pmr::vector<InnerResource>                        m_InnerResources;

    std::mutex                  m_ExecuteQueueMutex;
    std::pmr::vector<PassNode*> m_ExecuteQueue;

    utils::EnumArray<std::pmr::deque<std::pair<std::shared_ptr<Resource>, std::uint64_t>>, CommandType> m_RetiredResources;

    constexpr static unsigned                                                                 sm_CacheLifeSpan = 5;
    std::unordered_map<GpuBuffer::Desc, std::pair<std::shared_ptr<GpuBuffer>, std::uint64_t>> m_GpuBfferPool;
    std::unordered_map<Texture::Desc, std::pair<std::shared_ptr<Texture>, std::uint64_t>>     m_TexturePool;

    utils::EnumArray<std::pmr::list<std::shared_ptr<CommandContext>>, CommandType> m_ContextPool;

    std::shared_ptr<spdlog::logger> m_Logger;
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
