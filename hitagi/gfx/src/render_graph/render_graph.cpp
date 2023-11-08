#include <hitagi/render_graph/render_graph.hpp>
#include <hitagi/render_graph/resource_edge.hpp>
#include <hitagi/render_graph/resource_node.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/utils/logger.hpp>

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/enumerate.hpp>
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
        const auto index         = m_BlackBoard[node_type].at(_name);
        const auto resource_node = std::static_pointer_cast<ResourceNode>(m_Nodes[index]);

        if (resource_node->m_Resource != resource) {
            m_Logger->error("Import resource({}) failed: name {} already exists",
                            fmt::styled(resource->GetName(), fmt::fg(fmt::color::red)),
                            fmt::styled(name, fmt::fg(fmt::color::red)));
            return invalid_index;
        } else {
            return index;
        }
    }

    for (const auto& [index, node] : m_Nodes | ranges::views::enumerate) {
        if (auto resource_node = std::dynamic_pointer_cast<ResourceNode>(node);
            resource_node) {
            if (resource_node->m_Resource == resource) {
                if (!name.empty()) {
                    m_BlackBoard[node_type].emplace(name, index);
                }
                return index;
            }
        }
    }

    const auto index = m_Nodes.size();
    switch (node_type) {
        case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
            m_Nodes.emplace_back(std::make_shared<GPUBufferNode>(*this, std::move(resource), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::Texture:
            m_Nodes.emplace_back(std::make_shared<TextureNode>(*this, std::move(resource), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::Sampler:
            m_Nodes.emplace_back(std::make_shared<SamplerNode>(*this, std::move(resource), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::RenderPipeline:
            m_Nodes.emplace_back(std::make_shared<RenderPipelineNode>(*this, std::move(resource), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::ComputePipeline:
            m_Nodes.emplace_back(std::make_shared<ComputePipelineNode>(*this, std::move(resource), name));
            break;
        default:
            utils::unreachable();
    }
    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, index);
    }
    return index;
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

    const auto index = m_Nodes.size();
    switch (node_type) {
        case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
            m_Nodes.emplace_back(std::make_shared<GPUBufferNode>(*this, std::move(std::get<gfx::GPUBufferDesc>(desc)), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::Texture:
            m_Nodes.emplace_back(std::make_shared<TextureNode>(*this, std::move(std::get<gfx::TextureDesc>(desc)), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::Sampler:
            m_Nodes.emplace_back(std::make_shared<SamplerNode>(*this, std::move(std::get<gfx::SamplerDesc>(desc)), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::RenderPipeline:
            m_Nodes.emplace_back(std::make_shared<RenderPipelineNode>(*this, std::move(std::get<gfx::RenderPipelineDesc>(desc)), name));
            break;
        case hitagi::rg::RenderGraphNode::Type::ComputePipeline:
            m_Nodes.emplace_back(std::make_shared<ComputePipelineNode>(*this, std::move(std::get<gfx::ComputePipelineDesc>(desc)), name));
            break;
        default:
            utils::unreachable();
    }
    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, index);
    }
    return index;
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

    const auto index = m_Nodes.size();
    switch (type) {
        case RenderGraphNode::Type::GPUBuffer: {
            const auto buffer_node = std::static_pointer_cast<GPUBufferNode>(m_Nodes[resource_node_index]);
            m_Nodes.emplace_back(std::make_shared<GPUBufferNode>(buffer_node->MoveTo(GPUBufferHandle{index}, name)));
        } break;
        case RenderGraphNode::Type::Texture: {
            const auto texture_node = std::static_pointer_cast<TextureNode>(m_Nodes[resource_node_index]);
            m_Nodes.emplace_back(std::make_shared<TextureNode>(texture_node->MoveTo(TextureHandle{index}, name)));
        } break;
        default: {
            m_Logger->error("Move resource failed: resource is not a GPUBuffer or Texture");
            return invalid_index;
        } break;
    }
    if (!name.empty()) {
        m_BlackBoard[type].emplace(name, index);
    }
    return index;
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
    ZoneScopedN("Compile RenderGraph");

    const auto transpose_adjacency_list = [](const AdjacencyList& adjacency_list) {
        AdjacencyList result;
        for (const auto& [from, tos] : adjacency_list) {
            if (!result.contains(from)) result.emplace(from, AdjacencyList::mapped_type{});

            for (auto to : tos) {
                result[to].emplace(from);
            }
        }
        return result;
    };

    if (!m_PresentPassHandle) {
        m_Logger->info("RenderGraph has no present pass, so nothing will be rendered");
        return true;
    }

    if (!m_DataFlow.empty()) {
        m_Logger->info("RenderGraph has already been compiled");
        return true;
    }

    {
        ZoneScopedN("initialize data flow");

        m_DataFlow = ranges::views::iota(std::size_t{0}, m_Nodes.size())                                                           //
                     | ranges::views::transform([](auto handle) { return std::make_pair(handle, AdjacencyList::mapped_type{}); })  //
                     | ranges::to<AdjacencyList>();

        auto pass_nodes = m_Nodes                                                                              //
                          | ranges::views::enumerate                                                           //
                          | ranges::views::filter([](const auto& item) { return item.second->IsPassNode(); })  //
                          | ranges::views::transform([](const auto& item) {
                                return std::make_pair(item.first, std::static_pointer_cast<PassNode>(item.second));
                            });

        for (const auto& [pass_handle, pass_node] : pass_nodes) {
            for (const auto& [buffer_handle, buffer_edge] : pass_node->m_GPUBufferEdges) {
                if (buffer_edge.write) {
                    m_DataFlow.at(pass_handle).emplace(buffer_handle.index);
                } else {
                    m_DataFlow.at(buffer_handle.index).emplace(pass_handle);
                }
            }
            for (const auto& [texture_handle, texture_edge] : pass_node->m_TextureEdges) {
                if (texture_edge.write) {
                    m_DataFlow.at(pass_handle).emplace(texture_handle.index);
                } else {
                    m_DataFlow.at(texture_handle.index).emplace(pass_handle);
                }
            }
            for (const auto& [sampler_handle, sampler_edge] : pass_node->m_SamplerEdges) {
                m_DataFlow.at(sampler_handle.index).emplace(pass_handle);
            }
            for (const auto render_pipeline : pass_node->m_RenderPipelines) {
                m_DataFlow.at(render_pipeline.index).emplace(pass_handle);
            }
            for (const auto compute_pipeline : pass_node->m_ComputePipelines) {
                m_DataFlow.at(compute_pipeline.index).emplace(pass_handle);
            }
        }

        auto resource_nodes = m_Nodes                                                                                  //
                              | ranges::views::enumerate                                                               //
                              | ranges::views::filter([](const auto& item) { return item.second->IsResourceNode(); })  //
                              | ranges::views::transform([](const auto& item) {
                                    return std::make_pair(item.first, std::static_pointer_cast<ResourceNode>(item.second));
                                });

        for (const auto& [handle, resource_node] : resource_nodes) {
            if (resource_node->m_MoveTo == std::numeric_limits<std::size_t>::max()) continue;

            m_DataFlow.at(handle).emplace(resource_node->m_MoveTo);
        }
    }

    // remove unused nodes
    {
        auto dependency_data_flow = transpose_adjacency_list(m_DataFlow);

        auto unseen_nodes = ranges::views::iota(std::size_t{0}, m_Nodes.size())  //
                            | ranges::to<std::pmr::unordered_set<std::size_t>>();

        const std::function<void(std::size_t)> do_dfs = [&](std::size_t handle) {
            if (unseen_nodes.contains(handle))
                unseen_nodes.erase(handle);

            // Keep write resource node
            if (auto pass_node = std::dynamic_pointer_cast<PassNode>(m_Nodes[handle]);
                pass_node) {
                auto write_buffer_handles = pass_node->m_GPUBufferEdges                                                                  //
                                            | ranges::views::filter([](const auto& handle_edge) { return handle_edge.second.write; })    //
                                            | ranges::views::transform([](const auto& handle_edge) { return handle_edge.first.index; })  //
                                            | ranges::views::filter([&](auto buffer_handle) { return unseen_nodes.contains(buffer_handle); });
                for (auto write_buffer_handle : write_buffer_handles) {
                    unseen_nodes.erase(write_buffer_handle);
                }
                auto write_texture_handles = pass_node->m_TextureEdges                                                                    //
                                             | ranges::views::filter([](const auto& handle_edge) { return handle_edge.second.write; })    //
                                             | ranges::views::transform([](const auto& handle_edge) { return handle_edge.first.index; })  //
                                             | ranges::views::filter([&](auto texture_handle) { return unseen_nodes.contains(texture_handle); });

                for (auto write_texture_handle : write_texture_handles) {
                    unseen_nodes.erase(write_texture_handle);
                }
            }

            for (auto next_handle : dependency_data_flow.at(handle))
                do_dfs(next_handle);
        };

        do_dfs(m_PresentPassHandle.index);

        for (auto handle : unseen_nodes) {
            m_DataFlow.erase(handle);
        }
        for (auto& [handle, next_handles] : m_DataFlow) {
            next_handles = next_handles                                                                                    //
                           | ranges::views::filter([&](auto next_handle) { return !unseen_nodes.contains(next_handle); })  //
                           | ranges::to<AdjacencyList::mapped_type>();
        }

        dependency_data_flow = transpose_adjacency_list(m_DataFlow);

        // create edge:
        // resource_1 -- move --> resource_2         resource_1 -- new_edge -->  pass_2
        //      |                    |read               |                         | write
        //      |                    v         OR        |                         v
        //      +--- new_edge -->  pass_2                +--------- move --- >  resource_2

        auto move_edges = m_DataFlow                                                                                        //
                          | ranges::views::filter([&](const auto& item) { return m_Nodes[item.first]->IsResourceNode(); })  //
                          | ranges::views::transform([&](const auto& item) {
                                auto next_resource_handles = item.second  //
                                                             | ranges::views::filter([&](auto handle) {
                                                                   return m_Nodes[item.first]->IsResourceNode();
                                                               })  //
                                                             | ranges::to<AdjacencyList::mapped_type>();
                                return std::make_pair(item.first, std::move(next_resource_handles));
                            }) |
                          ranges::to<AdjacencyList>();

        for (const auto& [resource_1_handle, next_resource_handles] : move_edges) {
            for (const auto resource_2_handle : next_resource_handles) {
                auto read_pass_2_handles = m_DataFlow[resource_2_handle]                                                        //
                                           | ranges::views::filter([&](auto handle) { return m_Nodes[handle]->IsPassNode(); })  //
                                           | ranges::to<AdjacencyList::mapped_type>();

                auto write_pass_2_handles = dependency_data_flow[resource_2_handle]                                              //
                                            | ranges::views::filter([&](auto handle) { return m_Nodes[handle]->IsPassNode(); })  //
                                            | ranges::to<AdjacencyList::mapped_type>();
                if (write_pass_2_handles.size() > 1) {
                    utils::unreachable();
                }
                m_DataFlow[resource_1_handle].merge(read_pass_2_handles);
                m_DataFlow[resource_1_handle].merge(write_pass_2_handles);
            }
        }
    }

    AdjacencyList pass_flow;
    {
        ZoneScopedN("create pass flow");

        auto pass_handles = m_DataFlow             //
                            | ranges::views::keys  //
                            | ranges::views::filter([&](auto handle) { return m_Nodes[handle]->IsPassNode(); });

        for (auto pass_handle : pass_handles) {
            pass_flow[pass_handle] = m_DataFlow[pass_handle] | ranges::views::transform([&](auto resource_handle) { return m_DataFlow[resource_handle]; })  //
                                     | ranges::views::join                                                                                                  //
                                     | ranges::views::filter([&](auto handle) { return m_Nodes[handle]->IsPassNode(); })                                    //
                                     | ranges::to<AdjacencyList::mapped_type>();
        }
    }

    {
        ZoneScopedN("topology sort");
        auto in_degrees = pass_flow                                                                          //
                          | ranges::views::keys                                                              //
                          | ranges::views::transform([](auto handle) { return std::make_pair(handle, 0); })  //
                          | ranges::to<std::pmr::unordered_map<std::size_t, std::size_t>>();

        for (const auto& [handle, next_handles] : pass_flow) {
            for (auto next_handle : next_handles) {
                ++in_degrees.at(next_handle);
            }
        }

        auto start_nodes = in_degrees                                                                  //
                           | ranges::views::filter([](const auto& item) { return item.second == 0; })  //
                           | ranges::views::keys                                                       //
                           | ranges::to<std::pmr::vector<std::size_t>>();

        std::size_t num_visited_nodes = 0;
        while (!start_nodes.empty()) {
            auto& current_layer = m_ExecuteLayers.emplace_back(ExecuteLayer{});

            for (auto handle : start_nodes) {
                auto pass = std::static_pointer_cast<PassNode>(m_Nodes[handle]);
                current_layer[pass->GetCommandType()].emplace_back(handle);
            }

            std::pmr::vector<std::size_t> new_start_nodes;
            for (auto handle : start_nodes) {
                num_visited_nodes++;
                for (auto next_handle : pass_flow.at(handle)) {
                    if (--in_degrees.at(next_handle) == 0) {
                        new_start_nodes.emplace_back(next_handle);
                    }
                }
            }
            start_nodes = std::move(new_start_nodes);
        }

        if (num_visited_nodes != pass_flow.size()) {
            m_Logger->error("RenderGraph has cycle");
            m_DataFlow.clear();
            m_ExecuteLayers.clear();
            return false;
        }
    }

    {
        ZoneScopedN("Allocate resources");
        for (auto handle : m_DataFlow | ranges::views::keys) {
            m_Nodes[handle]->Initialize();
        }
    }

    if (false) {
        ZoneScopedN("Create barriers");

        for (auto pass_handle : pass_flow | ranges::views::keys) {
            auto pass_node = std::static_pointer_cast<PassNode>(m_Nodes[pass_handle]);
            for (const auto& [buffer_handle, buffer_edge] : pass_node->m_GPUBufferEdges) {
                pass_node->m_GPUBufferBarriers.emplace_back(
                    gfx::GPUBufferBarrier{
                        .src_access = gfx::BarrierAccess::None,
                        .dst_access = buffer_edge.access,
                        .src_stage  = gfx::PipelineStage::None,
                        .dst_stage  = buffer_edge.stage,
                        .buffer     = Resolve(buffer_handle),
                    });
            }
            for (const auto& [texture_handle, texture_edge] : pass_node->m_TextureEdges) {
                pass_node->m_TextureBarriers.emplace_back(
                    gfx::TextureBarrier{
                        .src_access = gfx::BarrierAccess::None,
                        .dst_access = texture_edge.access,
                        .src_stage  = gfx::PipelineStage::None,
                        .dst_stage  = texture_edge.stage,
                        .src_layout = gfx::TextureLayout::Unkown,
                        .dst_layout = texture_edge.layout,
                        .texture    = Resolve(texture_handle),
                    });
            }
        }

        const auto dependency_pass_flow = transpose_adjacency_list(pass_flow);

        const std::function<void(std::size_t)> do_dfs = [&](std::size_t current_pass_handle) {
            const auto pass_node = std::static_pointer_cast<PassNode>(m_Nodes[current_pass_handle]);
            for (auto prev_pass_handle : dependency_pass_flow.at(current_pass_handle)) {
                const auto prev_pass_node = std::static_pointer_cast<PassNode>(m_Nodes[prev_pass_handle]);

                for (auto& buffer_barrier : pass_node->m_GPUBufferBarriers) {
                    for (const auto& prev_buffer_barrier : prev_pass_node->m_GPUBufferBarriers) {
                        if (&buffer_barrier.buffer == &prev_buffer_barrier.buffer) {
                            buffer_barrier.src_access = prev_buffer_barrier.dst_access;
                            buffer_barrier.src_stage  = prev_buffer_barrier.dst_stage;
                        }
                    }
                }

                for (auto& texture_barrier : pass_node->m_TextureBarriers) {
                    for (const auto& prev_texture_barrier : prev_pass_node->m_TextureBarriers) {
                        if (&texture_barrier.texture == &prev_texture_barrier.texture) {
                            texture_barrier.src_access = prev_texture_barrier.dst_access;
                            texture_barrier.src_layout = prev_texture_barrier.dst_layout;
                            texture_barrier.src_stage  = prev_texture_barrier.dst_stage;
                        }
                    }
                }
            }
        };
        do_dfs(m_PresentPassHandle.index);

        // remove redundant barrier and modify barrier for d3d12 compatibility
        for (auto pass_handle : pass_flow | ranges::views::keys) {
            auto pass_node       = std::static_pointer_cast<PassNode>(m_Nodes[pass_handle]);
            auto buffer_barriers = pass_node->m_GPUBufferBarriers  //
                                   | ranges::views::filter([](const auto& barrier) {
                                         return barrier.src_access != barrier.dst_access &&
                                                barrier.src_stage != barrier.dst_stage;
                                     })  //
                                   | ranges::to<decltype(pass_node->m_GPUBufferBarriers)>();

            pass_node->m_GPUBufferBarriers.clear();
            std::copy(buffer_barriers.begin(), buffer_barriers.end(), std::back_inserter(pass_node->m_GPUBufferBarriers));

            auto texture_barriers = pass_node->m_TextureBarriers  //
                                    | ranges::views::filter([](const auto& barrier) {
                                          return barrier.src_access != barrier.dst_access &&
                                                 barrier.src_stage != barrier.dst_stage &&
                                                 barrier.src_layout != barrier.dst_layout;
                                      })  //
                                    | ranges::to<decltype(pass_node->m_TextureBarriers)>();

            pass_node->m_TextureBarriers.clear();
            std::copy(texture_barriers.begin(), texture_barriers.end(), std::back_inserter(pass_node->m_TextureBarriers));

            // https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
            if (m_Device.device_type == gfx::Device::Type::DX12) {
                if (pass_node->GetCommandType() == gfx::CommandType::Copy) {
                    for (auto& buffer_barrier : pass_node->m_GPUBufferBarriers) {
                        if (buffer_barrier.src_access != gfx::BarrierAccess::CopySrc ||
                            buffer_barrier.src_access != gfx::BarrierAccess::CopySrc) {
                            buffer_barrier.src_access = gfx::BarrierAccess::None;
                        }
                        if (buffer_barrier.src_stage != gfx::PipelineStage::Copy) {
                            buffer_barrier.src_stage = gfx::PipelineStage::None;
                        }
                    }
                    for (auto& texture_barrier : pass_node->m_TextureBarriers) {
                        if (texture_barrier.src_access != gfx::BarrierAccess::CopySrc ||
                            texture_barrier.src_access != gfx::BarrierAccess::CopySrc) {
                            texture_barrier.src_access = gfx::BarrierAccess::None;
                        }
                        if (texture_barrier.src_stage != gfx::PipelineStage::Copy) {
                            texture_barrier.src_stage = gfx::PipelineStage::None;
                        }
                        if (texture_barrier.src_layout != gfx::TextureLayout::Common) {
                            texture_barrier.src_layout = gfx::TextureLayout::Unkown;
                        }
                        texture_barrier.dst_layout = gfx::TextureLayout::Common;
                    }
                }
            }
        }
    }

    return true;
}

auto RenderGraph::Execute() -> std::uint64_t {
    ZoneScopedN("Execute RenderGraph");

    auto pass_nodes = m_ExecuteLayers | ranges::views::join | ranges::views::join  //
                      | ranges::views::transform([&](auto handle) {
                            return std::static_pointer_cast<PassNode>(m_Nodes.at(handle));
                        });

    {
        ZoneScopedN("Execute Pass");
        for (const auto& pass_node : pass_nodes) {
            pass_node->Execute();
        }
    }
    auto last_fences = m_Fences;

    {
        ZoneScopedN("Submit commands");

        for (const auto& execute_layer : m_ExecuteLayers) {
            // Improve more fine-grained fence
            magic_enum::enum_for_each<gfx::CommandType>([&](gfx::CommandType type) {
                auto pass_nodes = execute_layer[type]  //
                                  | ranges::views::transform([&](auto pass_handle) {
                                        return std::static_pointer_cast<PassNode>(m_Nodes.at(pass_handle));
                                    });
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
    }

    {
        ZoneScopedN("Wait for last fence");
        for (const auto& [fence, last_value] : last_fences) {
            fence->Wait(last_value);
        }
    }

    RetireNodes();
    Reset();

    return m_FrameIndex++;
}

void RenderGraph::Reset() {
    ZoneScopedN("Reset RenderGraph");

    m_PresentPassHandle = {};
    m_Nodes.clear();
    m_BlackBoard = {};
    m_DataFlow.clear();
    m_ExecuteLayers.clear();
}

void RenderGraph::RetireNodesFromPassNode(const std::shared_ptr<PassNode>& pass_node, const FenceValue& fence_value) {
    for (auto& [buffer_handle, buffer_edge] : pass_node->m_GPUBufferEdges) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes.at(buffer_handle.index),
            .last_fence_value = fence_value,
        });
    }

    for (const auto& [texture_handle, texture_edge] : pass_node->m_TextureEdges) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes.at(texture_handle.index),
            .last_fence_value = fence_value,
        });
    }

    for (const auto& [sampler_handle, sampler_edge] : pass_node->m_SamplerEdges) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes.at(sampler_handle.index),
            .last_fence_value = fence_value,
        });
    }

    m_RetiredNodes.emplace_back(RetiredNode{
        .node             = pass_node,
        .last_fence_value = fence_value,
    });

    for (const auto render_pipeline_handle : pass_node->m_RenderPipelines) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes.at(render_pipeline_handle.index),
            .last_fence_value = fence_value,
        });
    }

    for (const auto compute_pipeline_handle : pass_node->m_ComputePipelines) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes.at(compute_pipeline_handle.index),
            .last_fence_value = fence_value,
        });
    }
}

void RenderGraph::RetireNodes() {
    ZoneScopedN("RetireNodes");

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
    return AdjacencyListToDot(m_DataFlow);
}

auto RenderGraph::AdjacencyListToDot(const AdjacencyList& adjacency_list) const noexcept -> std::pmr::string {
    const auto node_writer = [&](std::size_t handle) {
        const auto&      node  = m_Nodes[handle];
        std::string_view shape = node->IsResourceNode() ? "shape=box " : "";
        return std::pmr::string(fmt::format(R"({}label="{}\nhandle: {}")", shape, node->GetName(), handle));
    };

    const auto edge_writer = [&](std::size_t from, std::size_t to) {
        std::shared_ptr<PassNode> pass_node;
        std::size_t               resource_handle;
        if (m_Nodes[from]->IsPassNode()) {
            pass_node       = std::static_pointer_cast<PassNode>(m_Nodes[from]);
            resource_handle = to;
        } else if (m_Nodes[to]->IsPassNode()) {
            pass_node       = std::static_pointer_cast<PassNode>(m_Nodes[to]);
            resource_handle = from;
        }

        if (pass_node) {
            std::pmr::string edge;
            for (const auto& [buffer_handle, buffer_edge] : pass_node->m_GPUBufferEdges) {
                if (buffer_handle == resource_handle) {
                    if (!(buffer_edge.write && resource_handle == from)) {
                        edge = fmt::format("{},{}", buffer_edge.access, buffer_edge.stage);
                    }
                    break;
                }
            }
            for (const auto& [texture_handle, texture_edge] : pass_node->m_TextureEdges) {
                if (texture_handle == resource_handle) {
                    if (!(texture_edge.write && resource_handle == from)) {
                        edge = fmt::format("{},{}", texture_edge.access, texture_edge.stage);
                    }
                    break;
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

    for (auto handle : adjacency_list | ranges::views::keys) {
        output += fmt::format("  {} [{}];\n", handle, node_writer(handle));
    }

    for (const auto& [handle, next_handles] : adjacency_list) {
        for (auto next_handle : next_handles) {
            output += fmt::format("  {} -> {} [{}];\n", handle, next_handle, edge_writer(handle, next_handle));
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