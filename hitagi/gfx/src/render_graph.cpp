#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/utils/utils.hpp>

#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/color.h>

#include <algorithm>

namespace hitagi::gfx {

RenderGraph::RenderGraph(Device& device)
    : device(device),
      m_Executor(),
      m_Logger(spdlog::stdout_color_mt(fmt::format("RenderGraph-{}", device.GetName()))) {
    magic_enum::enum_for_each<CommandType>([&](CommandType type) {
        m_Fences[type] = {device.CreateFence(0, fmt::format("RenderGraph Fence-{}", magic_enum::enum_name(type))), 0};
    });
}

auto RenderGraph::Builder::Create(ResourceDesc desc) const -> ResourceHandle {
    auto ret = Write(m_RenderGraph.CreateResource(desc));
    m_Node->writes.emplace(ret);
    return ret;
}

auto RenderGraph::Builder::Read(ResourceHandle input) const -> ResourceHandle {
    assert(input.id <= m_RenderGraph.m_ResourceNodes.size() && "Invalid resource handle");
    m_Node->reads.emplace(input);
    return input;
}

auto RenderGraph::Builder::Write(ResourceHandle output) const -> ResourceHandle {
    assert(output.id <= m_RenderGraph.m_ResourceNodes.size() && "Invalid resource handle");

    auto& old_res_node = m_RenderGraph.GetResrouceNode(output);
    // We must make sure the version `old_res_node` is newest now.
    if (auto iter = std::find_if(
            m_RenderGraph.m_ResourceNodes.rbegin(),
            m_RenderGraph.m_ResourceNodes.rend(),
            [&](const ResourceNode& res_node) -> bool {
                return res_node.res_idx == old_res_node.res_idx;
            });
        iter->version > old_res_node.version) {
        m_RenderGraph.m_Logger->warn(
            "Could write to old version({}) resource ({})",
            fmt::styled(old_res_node.version, fmt::fg(fmt::color::orange)),
            fmt::styled(m_RenderGraph.m_Resources[old_res_node.res_idx]->GetName(), fmt::fg(fmt::color::orange)));

        return ResourceHandle::InvalidHandle();
    }

    m_Node->reads.emplace(output);
    // Create new resource node
    auto& new_res_node   = m_RenderGraph.m_ResourceNodes.emplace_back(old_res_node.name, old_res_node.res_idx);
    new_res_node.writer  = m_Node;
    new_res_node.version = old_res_node.version + 1;

    output = {m_RenderGraph.m_ResourceNodes.size()};
    m_Node->writes.emplace(output);
    return output;
}

void RenderGraph::Builder::UseRenderPipeline(std::shared_ptr<RenderPipeline> pipeline) const {
    if (m_Node->type != CommandType::Graphics) {
        m_RenderGraph.m_Logger->warn(
            "Pass node (type: {}) can not use RenderPipeline({})",
            fmt::styled(magic_enum::enum_name(m_Node->type), fmt::fg(fmt::color::green)),
            fmt::styled(pipeline->GetName(), fmt::fg(fmt::color::orange)));
    }
    m_Node->render_pipelines.emplace_back(std::move(pipeline));
}

void RenderGraph::Builder::UseComputePipeline(std::shared_ptr<ComputePipeline> pipeline) const {
    if (m_Node->type != CommandType::Compute) {
        m_RenderGraph.m_Logger->warn(
            "Pass node (type: {}) can not use ComputePipeline({})",
            fmt::styled(magic_enum::enum_name(m_Node->type), fmt::fg(fmt::color::green)),
            fmt::styled(pipeline->GetName(), fmt::fg(fmt::color::orange)));
    }
    m_Node->compute_pipelines.emplace_back(std::move(pipeline));
}

void RenderGraph::PresentPass(ResourceHandle swap_chain) {
    auto present_pass  = std::make_shared<PassNodeWithData<GraphicsCommandContext, ResourceHandle>>();
    present_pass->name = "PresentPass";

    Builder builder(*this, present_pass.get());
    builder.Read(swap_chain);

    present_pass->executor = [swap_chain, helper = ResourceHelper(*this, present_pass.get())](GraphicsCommandContext* context) {
        context->Present(helper.Get<SwapChain>(swap_chain));
    };

    m_PresentPassNode = present_pass;
}

auto RenderGraph::Import(std::string_view name, std::shared_ptr<Resource> res) -> ResourceHandle {
    if (std::pmr::string _name = {name.begin(), name.end()}; m_BlackBoard.contains(_name)) {
        auto iter = std::find_if(
            m_ResourceNodes.rbegin(),
            m_ResourceNodes.rend(),
            [&, res_idx = m_BlackBoard.at(_name)](const ResourceNode& res_node) -> bool {
                return res_node.res_idx == res_idx;
            });
        return {static_cast<std::uint64_t>(std::distance(iter, m_ResourceNodes.rend()))};
    }

    if (std::find(m_OuterResources.begin(), m_OuterResources.end(), res) == m_OuterResources.end()) {
        m_OuterResources.emplace_back(res);
    }

    const std::size_t res_idx = m_Resources.size();
    m_Resources.emplace_back(res.get());
    m_BlackBoard.emplace(std::pmr::string(name), res_idx);
    m_ResourceNodes.emplace_back(name, res_idx);

    return ResourceHandle{m_ResourceNodes.size()};
}

auto RenderGraph::ImportWithoutLifeTrack(std::string_view name, Resource* res) -> ResourceHandle {
    if (std::pmr::string _name = {name.begin(), name.end()}; m_BlackBoard.contains(_name)) {
        auto iter = std::find_if(
            m_ResourceNodes.rbegin(),
            m_ResourceNodes.rend(),
            [&, res_idx = m_BlackBoard.at(_name)](const ResourceNode& res_node) -> bool {
                return res_node.res_idx == res_idx;
            });
        return {static_cast<std::uint64_t>(std::distance(iter, m_ResourceNodes.rend()))};
    }

    const std::size_t res_idx = m_Resources.size();
    m_Resources.emplace_back(res);
    m_BlackBoard.emplace(std::pmr::string(name), res_idx);
    m_ResourceNodes.emplace_back(name, res_idx);

    return ResourceHandle{m_ResourceNodes.size()};
}

auto RenderGraph::GetImportedResourceHandle(std::string_view name) -> ResourceHandle {
    if (const auto _name = std::pmr::string{name}; m_BlackBoard.contains(_name)) {
        std::size_t res_idx = m_BlackBoard[_name];

        // we need to find the newest resource node, so we use the reversed iterator
        // because the version of posted nodes always less than the previous nodes in the sub-array that all nodes point to the same resource
        auto iter = std::find_if(m_ResourceNodes.rbegin(), m_ResourceNodes.rend(), [&](const ResourceNode& node) {
            // Note that the following code does not use `node.name` to compare with `_name` because the `node.name` is not identifier from black board
            return node.res_idx == res_idx;
        });

        // note that the resource handle is start from 1, so the following code do not add -1
        return ResourceHandle{static_cast<std::uint64_t>(std::distance(iter, m_ResourceNodes.rend()))};
    }
    return ResourceHandle::InvalidHandle();
}

auto RenderGraph::CreateResource(ResourceDesc desc) -> ResourceHandle {
    // create new resource node

    std::string_view name = std::visit(
        utils::Overloaded{
            [](const GPUBufferDesc& _desc) {
                return _desc.name;
            },
            [](const TextureDesc& _desc) {
                return _desc.name;
            },
        },
        desc);

    std::size_t res_idx = m_Resources.size();
    m_Resources.emplace_back(nullptr);
    m_InnerResources.emplace_back(InnerResource{
        .desc           = desc,
        .resource_index = res_idx,
    });
    m_ResourceNodes.emplace_back(name, res_idx);

    return {m_ResourceNodes.size()};
}

auto RenderGraph::GetResrouceNode(ResourceHandle handle) -> ResourceNode& {
    return m_ResourceNodes.at(handle.id - 1);
}

auto RenderGraph::GetLifeTrackResource(const Resource* res) -> std::shared_ptr<Resource> {
    if (auto iter = std::find_if(m_InnerResources.begin(), m_InnerResources.end(), [res](const auto& inner_res) {
            return inner_res.resource.get() == res;
        });
        iter != m_InnerResources.end()) {
        return iter->resource;
    }
    if (auto iter = std::find_if(m_OuterResources.begin(), m_OuterResources.end(), [res](const auto& outer_res) {
            return outer_res.get() == res;
        });
        iter != m_OuterResources.end()) {
        return *iter;
    }

    return nullptr;
}

bool RenderGraph::Compile() {
    ZoneScopedN("RenderGraph::Compile");

    if (m_PresentPassNode == nullptr) return false;

    std::pmr::unordered_map<const PassNode*, tf::Task> task_map;

    // bottom-up build taskflow
    // TODO: batch command context submit
    std::function<tf::Task(PassNode*)> create_task = [&](PassNode* node) -> tf::Task {
        if (task_map.contains(node))
            return task_map[node];

        node->context = device.CreateCommandContext(node->type, node->name);

        auto task = m_Taskflow.emplace([=, this]() {
            ZoneScopedNS("NodePass", 10);
            ZoneName(node->name.c_str(), node->name.size());
            node->context->Begin();
            node->Execute();
            node->context->End();
            std::lock_guard lock{m_ExecuteQueueMutex};
            m_ExecuteQueue.emplace_back(node);
        });

        task.name(std::string(node->name));
        task_map.emplace(node, task);

        for (auto read_res : node->reads) {
            if (auto writer = GetResrouceNode(read_res).writer; writer != nullptr) {
                task.succeed(create_task(writer));
            }
        }
        return task;
    };

    {
        ZoneScopedN("Create Taskflow");
        create_task(m_PresentPassNode.get());
    }

    {
        ZoneScopedN("Create Inner Resource");
        for (auto&& inner_resource : m_InnerResources) {
            std::visit(
                utils::Overloaded{
                    [&](const GPUBufferDesc& buffer_desc) {
                        if (!m_GPUBufferPool.contains(buffer_desc)) {
                            m_GPUBufferPool.emplace(buffer_desc, std::pair{device.CreateGPUBuffer(buffer_desc), sm_CacheLifeSpan});
                        }
                        m_GPUBufferPool.at(buffer_desc).second     = sm_CacheLifeSpan;
                        inner_resource.resource                    = m_GPUBufferPool.at(buffer_desc).first;
                        m_Resources[inner_resource.resource_index] = inner_resource.resource.get();
                    },
                    [&](const TextureDesc& texture_desc) {
                        if (!m_TexturePool.contains(texture_desc)) {
                            m_TexturePool.emplace(texture_desc, std::pair{device.CreateTexture(texture_desc), sm_CacheLifeSpan});
                        }
                        m_TexturePool.at(texture_desc).second      = sm_CacheLifeSpan;
                        inner_resource.resource                    = m_TexturePool.at(texture_desc).first;
                        m_Resources[inner_resource.resource_index] = inner_resource.resource.get();
                    },
                },
                inner_resource.desc);
        }
    }

    {
        ZoneScopedN("Run PassNode");
        m_Executor.run(m_Taskflow).wait();
    }

    return true;
}

void RenderGraph::Execute() {
    ZoneScopedN("RenderGraph::Execute");

    // batch command context submit
    auto left  = m_ExecuteQueue.begin();
    auto right = left;
    while (left != m_ExecuteQueue.end()) {
        if (right != m_ExecuteQueue.end() && (*left)->type == (*right)->type) {
            right = std::next(right);
            continue;
        }

        std::pmr::vector<CommandContext*> contexts;

        auto wait_values = utils::create_enum_array<std::uint64_t, CommandType>(0);
        std::transform(left, right, std::back_inserter(contexts), [&](const PassNode* node) {
            // wait for resource writer who write to resources that this node read
            for (auto res_handle : node->reads) {
                auto& res_node = GetResrouceNode(res_handle);

                if (res_node.writer) {
                    // synchronize between different command queue
                    if (res_node.writer->type != node->type) {
                        wait_values[node->type] = m_Fences[node->type].counter;
                    }
                }
            }
            return node->context.get();
        });

        auto& fence_counter = m_Fences[(*left)->type];
        fence_counter.counter++;
        const FenceSignalInfo fence_signal_info{
            .fence = *fence_counter.fence,
            .value = fence_counter.counter,
        };
        auto& queue = device.GetCommandQueue((*left)->type);
        queue.Submit(
            contexts,
            // Improve dose there can use pipeline stage?
            {
                {*m_Fences[CommandType::Graphics].fence, wait_values[CommandType::Graphics]},
                {*m_Fences[CommandType::Compute].fence, wait_values[CommandType::Compute]},
                {*m_Fences[CommandType::Copy].fence, wait_values[CommandType::Copy]},
            },
            {fence_signal_info});

        const FenceWaitInfo resources_fence_wait_info{
            .fence = *fence_counter.fence,
            .value = fence_counter.counter,
        };
        std::for_each(left, right, [&](PassNode* node) {
            assert(node->type == queue.GetType());

            for (auto res_handle : node->reads) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources.emplace_back(track_res, resources_fence_wait_info);
                }
            }
            for (auto res_handle : node->writes) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources.emplace_back(track_res, resources_fence_wait_info);
                }
            }
            if (node->type == CommandType::Graphics) {
                for (auto render_pipeline : node->render_pipelines) {
                    m_RetiredResources.emplace_back(render_pipeline, resources_fence_wait_info);
                }
            } else if (node->type == CommandType::Compute) {
                for (auto compute_pipeline : node->compute_pipelines) {
                    m_RetiredResources.emplace_back(compute_pipeline, resources_fence_wait_info);
                }
            }
        });

        left = right;
    }

    TracyPlot("RetiredResources", static_cast<std::int64_t>(m_RetiredResources.size()));
}

void RenderGraph::Reset() {
    ZoneScoped;
    while (!m_RetiredResources.empty()) {
        const auto& [retired_res, fence_wait_pair] = m_RetiredResources.front();

        auto&& [fence, value, stage] = fence_wait_pair;
        if (fence.GetCurrentValue() < value) {
            m_RetiredResources.pop_front();
        }
    }

    auto decrease_counter_fn = [](auto& item) {
        auto& [res, life_counter] = item.second;

        life_counter = life_counter == 0 ? 0 : life_counter - 1;
    };
    std::for_each(m_GPUBufferPool.begin(), m_GPUBufferPool.end(), decrease_counter_fn);
    std::for_each(m_TexturePool.begin(), m_TexturePool.end(), decrease_counter_fn);

    auto discard_cache_fn = [](auto& item) {
        auto& [res, life_counter] = item.second;
        return life_counter == 0;
    };
    std::erase_if(m_GPUBufferPool, discard_cache_fn);
    std::erase_if(m_TexturePool, discard_cache_fn);

    m_ResourceNodes.clear();
    m_PassNodes.clear();
    m_PresentPassNode = nullptr;
    m_Resources.clear();
    m_BlackBoard.clear();
    m_InnerResources.clear();
    m_OuterResources.clear();
    m_ExecuteQueue.clear();
    m_Taskflow.clear();
}

}  // namespace hitagi::gfx