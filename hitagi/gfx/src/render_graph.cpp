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
      m_Logger(spdlog::stdout_color_mt("RenderGraph")) {
    magic_enum::enum_for_each<CommandType>([&](CommandType type) {
        m_SemaphoreWaitPairs[type] = {device.CreateSemaphore(), 0};
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

void RenderGraph::PresentPass(ResourceHandle back_buffer) {
    auto present_pass  = std::make_shared<PassNodeWithData<GraphicsCommandContext, ResourceHandle>>();
    present_pass->name = "PresentPass";

    Builder builder(*this, present_pass.get());
    builder.Read(back_buffer);

    present_pass->executor = [back_buffer, helper = ResourceHelper(*this, present_pass.get())](GraphicsCommandContext* context) {
        context->Present(helper.Get<Texture>(back_buffer));
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
            [](const GpuBuffer::Desc& _desc) {
                return _desc.name;
            },
            [](const Texture::Desc& _desc) {
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

auto RenderGraph::RequestCommandContext(CommandType type) -> std::shared_ptr<CommandContext> {
    std::shared_ptr<CommandContext> context = nullptr;
    switch (type) {
        case CommandType::Graphics:
            context = device.CreateGraphicsContext();
            break;
        case CommandType::Compute:
            context = device.CreateComputeContext();
            break;
        case CommandType::Copy:
            context = device.CreateCopyContext();
            break;
    }
    return context;
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

        node->context = RequestCommandContext(node->type);
        node->context->SetName(node->name);

        auto task = m_Taskflow.emplace([=, this]() {
            ZoneScopedNS("NodePass", 10);
            ZoneName(node->name.c_str(), node->name.size());
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
                    [&](const GpuBuffer::Desc& buffer_desc) {
                        if (!m_GpuBufferPool.contains(buffer_desc)) {
                            m_GpuBufferPool.emplace(buffer_desc, std::pair{device.CreateGpuBuffer(buffer_desc), sm_CacheLifeSpan});
                        }
                        m_GpuBufferPool.at(buffer_desc).second     = sm_CacheLifeSpan;
                        inner_resource.resource                    = m_GpuBufferPool.at(buffer_desc).first;
                        m_Resources[inner_resource.resource_index] = inner_resource.resource.get();
                    },
                    [&](const Texture::Desc& texture_desc) {
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

auto RenderGraph::Execute() -> utils::EnumArray<SemaphoreWaitPair, CommandType> {
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

        std::pmr::unordered_map<SemaphoreWaitPair::first_type, SemaphoreWaitPair::second_type> wait_semaphores;
        std::transform(left, right, std::back_inserter(contexts), [&](const PassNode* node) {
            // wait for resource writer who write to resources that this node read
            for (auto res_handle : node->reads) {
                auto& res_node = GetResrouceNode(res_handle);

                if (res_node.writer) {
                    // synchronize between different command queue
                    if (res_node.writer->type != node->type) {
                        wait_semaphores[m_SemaphoreWaitPairs[node->type].first] = m_SemaphoreWaitPairs[node->type].second;
                        m_SemaphoreWaitPairs[node->type].second++;
                    }
                }
            }
            return node->context.get();
        });

        auto& queue = device.GetCommandQueue((*left)->type);
        queue.Submit(std::move(contexts));

        std::for_each(left, right, [&](PassNode* node) {
            assert(node->type == queue.GetType());

            m_ContextPool[node->type].emplace_back(std::move(node->context));
            for (auto res_handle : node->reads) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources.emplace_back(track_res, fence);
                }
            }
            for (auto res_handle : node->writes) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources.emplace_back(track_res, fence);
                }
            }
            if (node->type == CommandType::Graphics) {
                for (auto render_pipeline : node->render_pipelines) {
                    m_RetiredResources.emplace_back(render_pipeline, fence);
                }
            } else if (node->type == CommandType::Compute) {
                for (auto compute_pipeline : node->compute_pipelines) {
                    m_RetiredResources.emplace_back(compute_pipeline, fence);
                }
            }
        });

        left = right;
    }
    magic_enum::enum_for_each<CommandType>([this](CommandType type) {
        std::pmr::string retired_res_message{magic_enum::enum_name(type).data()};
        for (const auto& [retired_res, fence_value] : m_RetiredResources[type]) {
            retired_res_message += fmt::format("{}, ", fence_value);
        }
        TracyPlot(magic_enum::enum_name(type).data(), static_cast<std::int64_t>(m_RetiredResources[type].size()));
        TracyMessage(retired_res_message.c_str(), retired_res_message.size());
    });

    return fence_values;
}

void RenderGraph::Reset() {
    ZoneScoped;

    magic_enum::enum_for_each<CommandType>([this](CommandType type) {
        auto& queue = device.GetCommandQueue(type);
        while (!m_RetiredResources[type].empty()) {
            const auto& [retired_res, fence_value] = m_RetiredResources[type].front();
            if (queue.IsFenceComplete(fence_value)) {
                m_RetiredResources[type].pop_front();
            } else {
                break;
            }
        }
    });

    auto decrese_conter_fn = [](auto& item) {
        auto& [res, life_conter] = item.second;

        life_conter = life_conter == 0 ? 0 : life_conter - 1;
    };
    std::for_each(m_GpuBufferPool.begin(), m_GpuBufferPool.end(), decrese_conter_fn);
    std::for_each(m_TexturePool.begin(), m_TexturePool.end(), decrese_conter_fn);

    auto discard_cache_fn = [](auto& item) {
        auto& [res, life_conter] = item.second;
        return life_conter == 0;
    };
    std::erase_if(m_GpuBufferPool, discard_cache_fn);
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