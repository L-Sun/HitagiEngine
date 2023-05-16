#pragma once
#include <hitagi/gfx/bindless.hpp>
#include <hitagi/gfx/sync.hpp>
#include <hitagi/gfx/device.hpp>

#include <unordered_set>

namespace hitagi::gfx {

struct ResourceHandle;

using ResourceDesc  = std::variant<GPUBufferDesc, TextureDesc>;
using DelayResource = std::variant<std::shared_ptr<Resource>, ResourceDesc>;

class ResourceNode;
class PassNodeBase;
class PassBuilderBase;
template <typename PassData, CommandType T>
class PassBuilder;
class ResourceHelper;

template <typename PassBuilderT, typename PassData>
using SetupFunc = std::function<void(PassBuilderT&, PassData&)>;

template <typename PassData, CommandType T>
using ExecFunc = std::function<void(const ResourceHelper&, const PassData&, ContextType<T>& context)>;

class RenderGraph {
public:
    RenderGraph(Device& device, std::string_view name);
    ~RenderGraph();

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    auto Import(std::shared_ptr<Resource> resource) -> ResourceHandle;
    auto Create(ResourceDesc desc) -> ResourceHandle;

    template <typename PassData, CommandType T>
    auto AddPass(std::string_view name, SetupFunc<PassBuilder<PassData, T>, PassData> setup_func, ExecFunc<PassData, T> exec_func) -> const PassData&;

    template <typename PassData>
    auto GetPassData(std::string_view name) const -> const PassData&;

    void AddPresentPass(ResourceHandle swap_chain);

    void Compile();
    void Execute();

    auto GetResourceNode(ResourceHandle handle) const -> const ResourceNode&;
    auto GetResource(ResourceHandle handle) const -> const DelayResource&;
    auto GetResourceName(ResourceHandle handle) const -> std::string_view;

private:
    friend PassBuilderBase;
    friend ResourceHelper;

    auto GetResourceNode(ResourceHandle handle) -> ResourceNode&;
    void InitializeResource();
    void WaitLastFrame();

    Device&                         m_Device;
    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    bool          m_IsCompiled = false;
    std::uint64_t m_FrameIndex = 0;

    std::pmr::vector<DelayResource>                                          m_Resources;
    std::pmr::vector<std::shared_ptr<Resource>>                              m_OuterResources;
    std::pmr::vector<ResourceNode>                                           m_ResourceNodes;
    std::pmr::unordered_map<std::string_view, std::shared_ptr<PassNodeBase>> m_PassNodes;

    struct FenceInfo {
        std::shared_ptr<Fence> fence;
        std::uint64_t          next_value = 1;
    };
    utils::EnumArray<FenceInfo, CommandType> m_Fences;

    using BatchPasses  = std::pmr::vector<PassNodeBase*>;
    using ExecuteLayer = utils::EnumArray<BatchPasses, CommandType>;
    std::pmr::vector<ExecuteLayer> m_CompiledExecuteLayers;
};

struct ResourceHandle {
    ResourceHandle(std::uint64_t index = std::numeric_limits<std::uint64_t>::max()) : node_index(index) {}

    constexpr operator const bool() const noexcept { return node_index != std::numeric_limits<std::uint64_t>::max(); }

    constexpr inline auto operator==(const ResourceHandle& rhs) const noexcept -> bool { return node_index == rhs.node_index; }
    constexpr inline auto operator!=(const ResourceHandle& rhs) const noexcept -> bool { return node_index != rhs.node_index; }

    constexpr inline auto operator<=>(const ResourceHandle&) const noexcept = default;

    std::uint64_t node_index = std::numeric_limits<std::uint64_t>::max();
};
}  // namespace hitagi::gfx

namespace std {
template <>
struct hash<hitagi::gfx::ResourceHandle> {
    constexpr std::size_t operator()(const hitagi::gfx::ResourceHandle& handle) const noexcept {
        return handle.node_index;
    }
};
}  // namespace std

namespace hitagi::gfx {
class ResourceNode {
public:
    ResourceNode(const ResourceNode&)     = delete;
    ResourceNode(ResourceNode&&) noexcept = default;

    inline auto GetHandle() const noexcept { return m_Handle; }
    inline auto GetVersion() const noexcept { return m_Version; }
    inline auto IsNewest() const noexcept { return !m_NextVersionHandle; }

    auto GetResource() const noexcept -> const DelayResource&;
    auto GetPrevVersion() const noexcept -> const ResourceNode&;
    auto GetNextVersion() const noexcept -> const ResourceNode&;

private:
    friend RenderGraph;
    friend PassBuilderBase;
    friend ResourceHelper;

    ResourceNode(const RenderGraph& render_graph, ResourceHandle resource_handle, std::size_t resource_index);

    const RenderGraph& m_RenderGraph;

    ResourceHandle m_Handle{};
    ResourceHandle m_PrevVersionHandle{};
    ResourceHandle m_NextVersionHandle{};

    std::uint32_t m_Version = 0;
    std::size_t   m_ResourceIndex;

    const PassNodeBase*                          m_Writer = nullptr;
    std::pmr::unordered_set<const PassNodeBase*> m_Readers;
};

class PassNodeBase {
public:
    PassNodeBase(const RenderGraph& render_graph, std::string_view name) : m_RenderGraph(render_graph), m_Name(name) {}
    virtual ~PassNodeBase() = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }

    auto Read(ResourceHandle handle, PipelineStage stage) noexcept -> ResourceHandle;
    auto Write(ResourceHandle handle, PipelineStage stage) -> ResourceHandle;

    auto GetInResourceHandles() const noexcept -> const std::pmr::vector<ResourceHandle>;

    bool IsDependOn(const PassNodeBase& other) const noexcept;

    virtual auto GetCommandType() const noexcept -> CommandType = 0;
    virtual void Execute()                                      = 0;

    void ResourceBarrier();

protected:
    friend RenderGraph;
    friend PassBuilderBase;
    friend ResourceHelper;

    const RenderGraph& m_RenderGraph;
    std::pmr::string   m_Name;

    struct ResourceAccessInfo {
        PipelineStage  stage;
        bool           write;
        BindlessHandle bindless_handle{};
    };

    std::pmr::unordered_map<ResourceHandle, ResourceAccessInfo> m_ReadResourceHandles;
    std::pmr::unordered_map<ResourceHandle, ResourceAccessInfo> m_WriteResourceHandles;

    std::shared_ptr<CommandContext> m_CommandContext;
};

template <typename PassData, CommandType T>
class PassNode : public PassNodeBase {
public:
    using PassNodeBase::PassNodeBase;
    inline auto GetCommandType() const noexcept -> CommandType final { return T; }
    inline auto GetPassData() const noexcept -> const PassData& { return m_Data; }

    void Execute() final;

protected:
    friend RenderGraph;
    friend PassBuilder<PassData, T>;

    PassData              m_Data = {};
    ExecFunc<PassData, T> m_Executor;

    // only used for render pass
    ResourceHandle m_RenderTarget;
};

class PassBuilderBase {
public:
    PassBuilderBase(RenderGraph& rg, PassNodeBase& pass) : m_RenderGraph(rg), m_PassNode(pass) {}

    auto Read(ResourceHandle resource_handle, PipelineStage stage = PipelineStage::All) -> ResourceHandle;
    auto Write(ResourceHandle resource_handle, PipelineStage stage = PipelineStage::All) -> ResourceHandle;

    RenderGraph&  m_RenderGraph;
    PassNodeBase& m_PassNode;
};

template <typename PassData, CommandType T>
class PassBuilder : public PassBuilderBase {
public:
    PassBuilder(RenderGraph& rg, PassNode<PassData, T>& pass) : PassBuilderBase(rg, pass) {}
};

template <typename PassData>
class PassBuilder<PassData, CommandType::Graphics> : public PassBuilderBase {
public:
    PassBuilder(RenderGraph& rg, PassNode<PassData, CommandType::Graphics>& pass) : PassBuilderBase(rg, pass) {}

    // only used by render pass
    auto SetRenderTarget(ResourceHandle resource_handle) -> ResourceHandle;
};

class ResourceHelper {
public:
    ResourceHelper(const RenderGraph& rg, const PassNodeBase& pass) : m_RenderGraph(rg), m_RenderPass(pass) {}

    template <typename T>
    auto Get(ResourceHandle resource_handle) const -> T&;

    // Only GPUBuffer and Texture resource can invoke this function.
    auto Get(ResourceHandle resource_handle) const -> BindlessHandle;

private:
    const RenderGraph&  m_RenderGraph;
    const PassNodeBase& m_RenderPass;
};

template <typename PassData, CommandType T>
auto RenderGraph::AddPass(std::string_view name, SetupFunc<PassBuilder<PassData, T>, PassData> setup_func, ExecFunc<PassData, T> exec_func) -> const PassData& {
    auto pass_node        = std::make_shared<PassNode<PassData, T>>(*this, name);
    pass_node->m_Executor = std::move(exec_func);
    auto builder          = PassBuilder<PassData, T>(*this, *pass_node);
    setup_func(builder, pass_node->m_Data);
    m_PassNodes.emplace(pass_node->GetName(), pass_node);
    return pass_node->GetPassData();
}

template <typename PassData>
auto RenderGraph::GetPassData(std::string_view name) const -> const PassData& {
    if (m_PassNodes.contains(name)) {
        auto pass_node = m_PassNodes.at(name);
        switch (pass_node->GetCommandType()) {
            case CommandType::Graphics: {
                auto render_node = std::dynamic_pointer_cast<PassNode<PassData, CommandType::Graphics>>(pass_node);
                if (render_node) {
                    return render_node->GetPassData();
                }
            }
            case CommandType::Compute: {
                auto compute_node = std::dynamic_pointer_cast<PassNode<PassData, CommandType::Compute>>(pass_node);
                if (compute_node) {
                    return compute_node->GetPassData();
                }
            }
            case CommandType::Copy: {
                auto copy_node = std::dynamic_pointer_cast<PassNode<PassData, CommandType::Copy>>(pass_node);
                if (copy_node) {
                    return copy_node->GetPassData();
                }
            }
        }
    }
    throw std::runtime_error(fmt::format("Pass {} not found", name));
}

template <typename PassData, CommandType T>
void PassNode<PassData, T>::Execute() {
    ResourceHelper helper(this->m_RenderGraph, *this);
    m_CommandContext->Begin();
    ResourceBarrier();
    if (T == CommandType::Graphics && m_RenderTarget) {
        auto& gfx_context = *std::static_pointer_cast<GraphicsCommandContext>(m_CommandContext);
        auto& resource    = helper.Get<Resource>(m_RenderTarget);
        if (resource.GetType() == ResourceType::Texture) {
            gfx_context.BeginRendering({
                .render_target = static_cast<Texture&>(resource),
            });
        } else {
            gfx_context.BeginRendering({
                .render_target = static_cast<SwapChain&>(resource),
            });
        }
        m_Executor(helper, this->m_Data, gfx_context);
        gfx_context.EndRendering();
    } else {
        m_Executor(helper, this->m_Data, *std::static_pointer_cast<ContextType<T>>(m_CommandContext));
    }
    m_CommandContext->End();
}

template <typename PassData>
auto PassBuilder<PassData, CommandType::Graphics>::SetRenderTarget(ResourceHandle resource_handle) -> ResourceHandle {
    auto new_resource_handle = Write(resource_handle, PipelineStage::Render);

    static_cast<PassNode<PassData, CommandType::Graphics>&>(m_PassNode).m_RenderTarget = new_resource_handle;
    return new_resource_handle;
}

template <typename T>
auto ResourceHelper::Get(ResourceHandle handle) const -> T& {
    if (m_RenderPass.m_ReadResourceHandles.contains(handle)) {
        auto& resource = m_RenderGraph.GetResource(handle);
        return *std::dynamic_pointer_cast<T>(std::get<std::shared_ptr<Resource>>(resource));
    } else if (m_RenderPass.m_WriteResourceHandles.contains(handle)) {
        auto& resource = m_RenderGraph.GetResource(handle);
        return *std::dynamic_pointer_cast<T>(std::get<std::shared_ptr<Resource>>(resource));
    } else {
        throw std::runtime_error(fmt::format("Resource {} is not used in pass {}", handle.node_index, m_RenderPass.GetName()));
    }
}

}  // namespace hitagi::gfx
