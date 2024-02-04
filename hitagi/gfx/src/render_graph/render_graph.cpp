#include <hitagi/render_graph/render_graph.hpp>
#include <hitagi/render_graph/resource_edge.hpp>
#include <hitagi/render_graph/resource_node.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/utils/logger.hpp>

#include <fmt/color.h>
#include <magic_enum_utility.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/enumerate.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>

namespace hitagi::rg {

RenderGraph::RenderGraph(gfx::Device& device, std::string_view name)
    : m_Device(device),
      m_Name(name),
      m_Logger(utils::try_create_logger(fmt::format("RenderGraph{}", utils::add_parentheses(name)))) {
    m_Fences[gfx::CommandType::Graphics] = {
        .fence      = m_Device.CreateFence(0, "[RG] Render Fence"),
        .last_value = 0,
    };
    m_Fences[gfx::CommandType::Compute] = {
        .fence      = m_Device.CreateFence(0, "[RG] Compute Fence"),
        .last_value = 0,
    };
    m_Fences[gfx::CommandType::Copy] = {
        .fence      = m_Device.CreateFence(0, "[RG] Copy Fence"),
        .last_value = 0,
    };
}

RenderGraph::~RenderGraph() {
    m_Device.WaitIdle();
}

auto RenderGraph::ImportResource(std::shared_ptr<gfx::Resource> resource, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    if (resource == nullptr) {
        m_Logger->error("Import resource failed: resource is nullptr");
        return invalid_index;
    }

    const std::pmr::string _name(name);
    const auto             node_type = gfx_resource_type_to_node_type(resource->GetType());

    if (m_BlackBoard[node_type].contains(_name)) {
        const auto handle        = m_BlackBoard[node_type].at(_name);
        const auto resource_node = std::static_pointer_cast<ResourceNode>(m_Nodes[handle]);

        if (resource_node->m_Resource != resource) {
            m_Logger->error("Import resource({}) failed: name {} already exists",
                            fmt::styled(resource->GetName(), fmt::fg(fmt::color::red)),
                            fmt::styled(name, fmt::fg(fmt::color::red)));
            return invalid_index;
        } else {
            return handle;
        }
    }

    if (m_ImportedResources.contains(resource)) {
        if (!name.empty()) {
            m_BlackBoard[node_type].emplace(name, m_ImportedResources.at(resource));
        }
        return m_ImportedResources.at(resource);
    }

    std::shared_ptr<ResourceNode> new_node;
    {
        ZoneScopedN("create new resource node");
        switch (node_type) {
            case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
                new_node = std::make_shared<GPUBufferNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::Texture:
                new_node = std::make_shared<TextureNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::Sampler:
                new_node = std::make_shared<SamplerNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::RenderPipeline:
                new_node = std::make_shared<RenderPipelineNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::ComputePipeline:
                new_node = std::make_shared<ComputePipelineNode>(*this, resource, name);
                break;
            default:
                utils::unreachable();
        }
    }
    new_node->m_Handle = m_Nodes.size();

    m_Nodes.emplace_back(new_node);
    m_ImportedResources.emplace(resource, new_node->m_Handle);

    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, new_node->m_Handle);
    }

    return new_node->m_Handle;
}

auto RenderGraph::Import(std::shared_ptr<gfx::GPUBuffer> buffer, std::string_view name) noexcept -> GPUBufferHandle {
    return ImportResource(std::move(buffer), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::Texture> texture, std::string_view name) noexcept -> TextureHandle {
    return ImportResource(std::move(texture), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::Sampler> sampler, std::string_view name) noexcept -> SamplerHandle {
    return ImportResource(std::move(sampler), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::RenderPipeline> pipeline, std::string_view name) noexcept -> RenderPipelineHandle {
    return ImportResource(std::move(pipeline), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::ComputePipeline> pipeline, std::string_view name) noexcept -> ComputePipelineHandle {
    return ImportResource(std::move(pipeline), name);
}

auto RenderGraph::CreateResource(ResourceDesc desc, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    RenderGraphNode::Type node_type;

    if (std::holds_alternative<gfx::GPUBufferDesc>(desc)) {
        node_type = RenderGraphNode::Type::GPUBuffer;
    } else if (std::holds_alternative<gfx::TextureDesc>(desc)) {
        node_type = RenderGraphNode::Type::Texture;
    } else if (std::holds_alternative<gfx::SamplerDesc>(desc)) {
        node_type = RenderGraphNode::Type::Sampler;
    } else if (std::holds_alternative<gfx::RenderPipelineDesc>(desc)) {
        node_type = RenderGraphNode::Type::RenderPipeline;
    } else if (std::holds_alternative<gfx::ComputePipelineDesc>(desc)) {
        node_type = RenderGraphNode::Type::ComputePipeline;
    }

    const std::pmr::string _name(name);

    if (m_BlackBoard[node_type].contains(_name)) {
        m_Logger->error("Create buffer failed: name {} already exists",
                        fmt::styled(name, fmt::fg(fmt::color::red)));
        return invalid_index;
    }

    std::shared_ptr<ResourceNode> new_node;
    switch (node_type) {
        case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
            new_node = std::make_shared<GPUBufferNode>(*this, std::move(std::get<gfx::GPUBufferDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::Texture:
            new_node = std::make_shared<TextureNode>(*this, std::move(std::get<gfx::TextureDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::Sampler:
            new_node = std::make_shared<SamplerNode>(*this, std::move(std::get<gfx::SamplerDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::RenderPipeline:
            new_node = std::make_shared<RenderPipelineNode>(*this, std::move(std::get<gfx::RenderPipelineDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::ComputePipeline:
            new_node = std::make_shared<ComputePipelineNode>(*this, std::move(std::get<gfx::ComputePipelineDesc>(desc)), name);
            break;
        default:
            utils::unreachable();
    }

    new_node->m_Handle = m_Nodes.size();
    m_Nodes.emplace_back(new_node);

    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, new_node->m_Handle);
    }

    return new_node->m_Handle;
}

auto RenderGraph::Create(gfx::GPUBufferDesc desc, std::string_view name) noexcept -> GPUBufferHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::TextureDesc desc, std::string_view name) noexcept -> TextureHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::SamplerDesc desc, std::string_view name) noexcept -> SamplerHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::RenderPipelineDesc desc, std::string_view name) noexcept -> RenderPipelineHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::ComputePipelineDesc desc, std::string_view name) noexcept -> ComputePipelineHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::MoveFrom(RenderGraphNode::Type type, std::size_t resource_node_index, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    if (!IsValid(type, resource_node_index)) {
        m_Logger->error("Move resource failed: resource is invalid");
        return invalid_index;
    }

    const std::pmr::string _name(name);
    if (m_BlackBoard[type].contains(_name)) {
        m_Logger->error("Move resource failed: name {} already exists",
                        fmt::styled(name, fmt::fg(fmt::color::red)));
        return invalid_index;
    }

    const auto new_handle = m_Nodes.size();
    switch (type) {
        case RenderGraphNode::Type::GPUBuffer: {
            const auto buffer_node = std::static_pointer_cast<GPUBufferNode>(m_Nodes[resource_node_index]);
            m_Nodes.emplace_back(buffer_node->Move(new_handle, name));
        } break;
        case RenderGraphNode::Type::Texture: {
            const auto texture_node = std::static_pointer_cast<TextureNode>(m_Nodes[resource_node_index]);
            m_Nodes.emplace_back(texture_node->Move(new_handle, name));
        } break;
        default: {
            m_Logger->error("Move resource failed: resource is not a GPUBuffer or Texture");
            return invalid_index;
        } break;
    }
    return new_handle;
}

auto RenderGraph::GetBufferHandle(std::string_view name) const noexcept -> GPUBufferHandle {
    return GetHandle<RenderGraphNode::Type::GPUBuffer>(name);
}

auto RenderGraph::GetTextureHandle(std::string_view name) const noexcept -> TextureHandle {
    return GetHandle<RenderGraphNode::Type::Texture>(name);
}

auto RenderGraph::GetSamplerHandle(std::string_view name) const noexcept -> SamplerHandle {
    return GetHandle<RenderGraphNode::Type::Sampler>(name);
}

auto RenderGraph::GetRenderPipelineHandle(std::string_view name) const noexcept -> RenderPipelineHandle {
    return GetHandle<RenderGraphNode::Type::RenderPipeline>(name);
}

auto RenderGraph::GetComputePipelineHandle(std::string_view name) const noexcept -> ComputePipelineHandle {
    return GetHandle<RenderGraphNode::Type::ComputePipeline>(name);
}

bool RenderGraph::Compile() {
    ZoneScoped;

    if (m_Compiled) {
        m_Logger->info("RenderGraph has already been compiled");
        return true;
    }

    if (m_PresentPassNode == nullptr) {
        m_Logger->trace("RenderGraph has no present pass, so nothing will be rendered");
        m_Compiled = true;
        return true;
    }

    if (m_PresentPassNode->swap_chain->GetWidth() == 0 ||
        m_PresentPassNode->swap_chain->GetHeight() == 0) {
        m_Logger->trace("The window is minimized, so nothing will be rendered");
        m_Compiled = true;
        return true;
    }

    std::pmr::unordered_set<RenderGraphNode*> essential_nodes;
    {
        const std::function<void(RenderGraphNode*)> do_dfs = [&](RenderGraphNode* node) {
            if (essential_nodes.contains(node)) return;
            essential_nodes.emplace(node);

            for (auto input_node : node->m_InputNodes) {
                do_dfs(input_node);
            }
        };
        do_dfs(m_PresentPassNode.get());

        // we need keep all output resource node in essential pass node to avoid execution failure
        auto write_resource_nodes = essential_nodes                                                               //
                                    | ranges::views::filter([](const auto& node) { return node->IsPassNode(); })  //
                                    | ranges::views::transform([](auto node) { return node->m_OutputNodes; })     //
                                    | ranges::views::join                                                         //
                                    | ranges::to<std::pmr::unordered_set<RenderGraphNode*>>();
        essential_nodes.merge(write_resource_nodes);
    }

    {
        auto in_degrees = essential_nodes  //
                          | ranges::views::transform([](const auto& node) {
                                return std::make_pair(node, node->m_InputNodes.size());
                            })  //
                          | ranges::to<std::pmr::unordered_map<RenderGraphNode*, std::size_t>>();

        // use vector is ok
        auto start_nodes = in_degrees                                                                  //
                           | ranges::views::filter([](const auto& item) { return item.second == 0; })  //
                           | ranges::views::keys                                                       //
                           | ranges::to<std::pmr::vector<RenderGraphNode*>>();

        std::size_t num_visited_nodes = 0;
        while (!start_nodes.empty()) {
            ExecuteLayer current_layer;

            ranges::for_each(
                start_nodes                                                                             //
                    | ranges::views::filter([](const auto& node) { return node->IsPassNode(); })        //
                    | ranges::views::transform([](auto node) { return static_cast<PassNode*>(node); })  //
                ,
                [&](auto node) { current_layer[node->GetCommandType()].emplace_back(node); });

            m_ExecuteLayers.emplace_back(std::move(current_layer));

            std::pmr::vector<RenderGraphNode*> new_start_nodes;
            for (auto node : start_nodes) {
                num_visited_nodes++;
                for (auto output_node : node->m_OutputNodes) {
                    if (--in_degrees.at(output_node) == 0) {
                        new_start_nodes.emplace_back(output_node);
                    }
                }
            }
            start_nodes = std::move(new_start_nodes);
        }

        if (num_visited_nodes != essential_nodes.size()) {
            m_Logger->error("RenderGraph has cycle");
            m_ExecuteLayers.clear();
            return false;
        }
    }

    for (auto node : essential_nodes) {
        node->Initialize();
    }

    m_Compiled = true;

    return true;
}

auto RenderGraph::Execute() -> std::uint64_t {
    ZoneScoped;

    if (!m_Compiled) {
        m_Logger->warn("RenderGraph has not been compiled");
        return m_FrameIndex;
    }

    for (const auto& pass_node : m_ExecuteLayers | ranges::views::join | ranges::views::join) {
        pass_node->Execute();
    }
    auto last_fences = m_Fences;

    for (const auto& execute_layer : m_ExecuteLayers) {
        // Improve more fine-grained fence
        magic_enum::enum_for_each<gfx::CommandType>([&](gfx::CommandType type) {
            const auto& pass_nodes = execute_layer[type];

            if (pass_nodes.empty()) return;

            auto commands = pass_nodes  //
                            | ranges::views::transform([&](const auto& pass_node) {
                                  return std::cref(*pass_node->m_CommandContext);
                              })  //
                            | ranges::to<std::pmr::vector<std::reference_wrapper<const gfx::CommandContext>>>();

            std::pmr::vector<gfx::FenceWaitInfo> wait_fences;
            for (const auto& curr_fence_value : m_Fences) {
                wait_fences.emplace_back(*curr_fence_value.fence, curr_fence_value.last_value);
            }

            m_Device.GetCommandQueue(type).Submit(
                commands,
                wait_fences,
                {{gfx::FenceSignalInfo{
                    .fence = *m_Fences[type].fence,
                    .value = ++m_Fences[type].last_value,
                }}});

            // retire resource
            for (auto pass_node : pass_nodes) {
                RetireNodesFromPassNode(pass_node, m_Fences[type]);
            }
        });
    }

    for (const auto& [fence, last_value] : last_fences) {
        fence->Wait(last_value);
    }

    RetireNodes();
    Reset();
    return m_FrameIndex++;
}

void RenderGraph::Reset() noexcept {
    m_Compiled        = false;
    m_PresentPassNode = nullptr;
    m_Nodes.clear();
    m_ImportedResources.clear();
    for (auto& black_board : m_BlackBoard) {
        black_board.clear();
    }
    m_ExecuteLayers.clear();
}

void RenderGraph::RetireNodesFromPassNode(PassNode* pass_node, const FenceValue& fence_value) noexcept {
    for (auto input_node : pass_node->m_InputNodes) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes[input_node->m_Handle],
            .last_fence_value = fence_value,
        });
    }
    for (auto output_node : pass_node->m_OutputNodes) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes[output_node->m_Handle],
            .last_fence_value = fence_value,
        });
    }

    m_RetiredNodes.emplace_back(RetiredNode{
        .node             = m_Nodes[pass_node->m_Handle],
        .last_fence_value = fence_value,
    });
}

void RenderGraph::RetireNodes() noexcept {
    const auto latest_fence_values = m_Fences  //
                                     | ranges::views::transform([](const auto& fence_value) {
                                           return std::make_pair(fence_value.fence, fence_value.fence->GetCurrentValue());
                                       })  //
                                     | ranges::to<std::pmr::unordered_map<std::shared_ptr<gfx::Fence>, std::uint64_t>>();

    while (!m_RetiredNodes.empty()) {
        const auto& retired_resource    = m_RetiredNodes.front();
        const auto& [fence, last_value] = retired_resource.last_fence_value;
        if (latest_fence_values.at(fence) >= last_value) {
            m_RetiredNodes.pop_front();
        } else {
            break;
        }
    }
}

auto RenderGraph::ToDot() const noexcept -> std::pmr::string {
    const auto node_writer = [&](const RenderGraphNode* node) {
        std::string_view shape = node->IsResourceNode() ? "shape=box " : "";
        return std::pmr::string(fmt::format(R"({}label="{}\nhandle: {}")", shape, node->GetName(), node->m_Handle));
    };

    const auto edge_writer = [&](const RenderGraphNode* from, const RenderGraphNode* to) {
        const PassNode*     pass_node     = nullptr;
        const ResourceNode* resource_node = nullptr;
        if (from->IsPassNode()) {
            pass_node     = static_cast<const PassNode*>(from);
            resource_node = static_cast<const ResourceNode*>(to);
        } else if (to->IsPassNode()) {
            pass_node     = static_cast<const PassNode*>(to);
            resource_node = static_cast<const ResourceNode*>(from);
        }

        if (pass_node) {
            std::pmr::string edge;
            switch (resource_node->GetType()) {
                case RenderGraphNode::Type::GPUBuffer: {
                    const auto  buffer_node = const_cast<GPUBufferNode*>(static_cast<const GPUBufferNode*>(resource_node));
                    const auto& buffer_edge = pass_node->m_GPUBufferEdges.at(buffer_node);
                    if (!(buffer_edge.write && resource_node == from)) {
                        edge = fmt::format("{},{}", buffer_edge.access, buffer_edge.stage);
                    }
                } break;
                case RenderGraphNode::Type::Texture: {
                    const auto  texture_node = const_cast<TextureNode*>(static_cast<const TextureNode*>(resource_node));
                    const auto& texture_edge = pass_node->m_TextureEdges.at(texture_node);
                    if (!(texture_edge.write && resource_node == from)) {
                        edge = fmt::format("{},{},{}", texture_edge.access, texture_edge.layout, texture_edge.stage);
                    }
                } break;
                default: {
                }
            }
            return std::pmr::string(fmt::format("label=\"{}\"", edge));
        }
        // move edge
        else {
            return std::pmr::string("style=dashed");
        }
    };

    std::pmr::string output = "digraph {\n";

    for (const auto& node : m_Nodes) {
        output += fmt::format("  {} [{}];\n", node->m_Handle, node_writer(node.get()));
    }

    for (const auto& from_node : m_Nodes) {
        for (auto to_node : from_node->m_OutputNodes) {
            output += fmt::format(
                "  {} -> {} [{}];\n",
                from_node->m_Handle, to_node->m_Handle,
                edge_writer(from_node.get(), to_node));
        }
    }

    output += "}\n";
    return output;
}

void RenderGraph::Profile() const noexcept {
    static bool configured = false;
    if (!configured) {
        TracyPlotConfig("Retired Resource Counts", tracy::PlotFormatType::Number, true, true, 0);
        configured = true;
    }
    TracyPlot("Retired Resource Counts", static_cast<std::int64_t>(m_RetiredNodes.size()));
}

}  // namespace hitagi::rg