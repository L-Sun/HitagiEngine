#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>
#include <hitagi/utils/exceptions.hpp>
#include <hitagi/utils/utils.hpp>

#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>

#include <algorithm>

namespace hitagi::gfx {

auto RenderGraph::Builder::Create(ResourceDesc desc) const noexcept -> ResourceHandle {
    auto ret = Write(m_RenderGraph.CreateResource(desc));
    m_Node->writes.emplace(ret);
    return ret;
}

auto RenderGraph::Builder::Read(ResourceHandle input) const noexcept -> ResourceHandle {
    m_Node->reads.emplace(input);
    return input;
}

auto RenderGraph::Builder::Write(ResourceHandle output) const noexcept -> ResourceHandle {
    m_Node->reads.emplace(output);

    ResourceNode& old_res_node = m_RenderGraph.GetResrouceNode(output);
    // Create new resource node
    auto& new_res_node   = m_RenderGraph.m_ResourceNodes.emplace_back(old_res_node.name, old_res_node.res_idx);
    new_res_node.writer  = m_Node;
    new_res_node.version = old_res_node.version + 1;

    output = {m_RenderGraph.m_ResourceNodes.size()};
    m_Node->writes.emplace(output);
    return output;
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
    if (std::find(m_OutterResources.begin(), m_OutterResources.end(), res) == m_OutterResources.end()) {
        m_OutterResources.emplace_back(res);
    }
    return ImportWithoutLifeTrack(name, res.get());
}

auto RenderGraph::ImportWithoutLifeTrack(std::string_view name, Resource* res) -> ResourceHandle {
    if (m_BlackBoard.contains(std::pmr::string(name))) {
        auto iter = std::find_if(m_ResourceNodes.begin(),
                                 m_ResourceNodes.end(),
                                 [&](const auto& res_node) {
                                     return res == m_Resources[res_node.res_idx];
                                 });

        assert(iter != m_ResourceNodes.end());
        return {std::distance(m_ResourceNodes.begin(), iter) + 1ull};
    }

    std::size_t res_idx = m_Resources.size();
    m_Resources.emplace_back(res);
    m_BlackBoard.emplace(std::pmr::string(name), res_idx);
    m_ResourceNodes.emplace_back(name, res_idx);
    return {m_ResourceNodes.size()};
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
    if (m_ContextPool[type].empty() ||
        !device.GetCommandQueue(type)->IsFenceComplete(m_ContextPool[type].front()->fence_value)) {
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
    } else {
        context = std::move(m_ContextPool[type].front());
        m_ContextPool[type].pop_front();
        context->Reset();
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
    if (auto iter = std::find_if(m_OutterResources.begin(), m_OutterResources.end(), [res](const auto& outer_res) {
            return outer_res.get() == res;
        });
        iter != m_OutterResources.end()) {
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
        ZoneScopedN("Create Taksflow");
        create_task(m_PresentPassNode.get());
    }

    {
        ZoneScopedN("Create Inner Resource");
        for (auto&& inner_resource : m_InnerResources) {
            std::visit(
                utils::Overloaded{
                    [&](const GpuBuffer::Desc& buffer_desc) {
                        if (!m_GpuBfferPool.contains(buffer_desc)) {
                            m_GpuBfferPool.emplace(buffer_desc, device.CreateBuffer(buffer_desc));
                        }
                        inner_resource.resource                    = m_GpuBfferPool.at(buffer_desc);
                        m_Resources[inner_resource.resource_index] = inner_resource.resource.get();
                    },
                    [&](const Texture::Desc& texture_desc) {
                        if (!m_TexturePool.contains(texture_desc)) {
                            m_TexturePool.emplace(texture_desc, device.CreateTexture(texture_desc));
                        }
                        inner_resource.resource                    = m_TexturePool.at(texture_desc);
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

auto RenderGraph::Execute() -> utils::EnumArray<std::uint64_t, CommandType> {
    ZoneScopedN("RenderGraph::Execute");

    std::pmr::vector<CommandContext*> contexts;
    auto                              fence_values = utils::create_enum_array<std::uint64_t, CommandType>(0);

    auto left  = m_ExecuteQueue.begin();
    auto right = left;
    while (left != m_ExecuteQueue.end()) {
        if (right != m_ExecuteQueue.end() && (*left)->type == (*right)->type) {
            right = std::next(right);
            continue;
        }

        auto queue = device.GetCommandQueue((*left)->type);

        auto waited = utils::create_enum_array<bool, CommandType>(false);
        std::transform(left, right, std::back_inserter(contexts), [&](const PassNode* node) {
            for (auto res_handle : node->reads) {
                auto& res_node = GetResrouceNode(res_handle);
                if (res_node.writer && res_node.writer->type != node->type && !waited[res_node.writer->type]) {
                    queue->WaitForQueue(*device.GetCommandQueue(res_node.writer->type));
                    waited[res_node.writer->type] = true;
                }
            }
            return node->context.get();
        });

        fence_values[queue->type] = queue->Submit(contexts);

        std::for_each(left, right, [&](PassNode* node) {
            m_ContextPool[node->type].emplace_back(std::move(node->context));
            for (auto res_handle : node->reads) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources[queue->type].emplace_back(track_res, fence_values[queue->type]);
                }
            }
            for (auto res_handle : node->writes) {
                auto res       = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                auto track_res = GetLifeTrackResource(res);
                if (track_res) {
                    m_RetiredResources[queue->type].emplace_back(track_res, fence_values[queue->type]);
                }
            }
        });

        left = right;
        contexts.clear();
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
        auto queue = device.GetCommandQueue(type);
        while (!m_RetiredResources[type].empty()) {
            const auto& [retired_res, fence_value] = m_RetiredResources[type].front();
            if (queue->IsFenceComplete(fence_value)) {
                m_RetiredResources[type].pop_front();
            } else {
                break;
            }
        }
    });

    m_ResourceNodes.clear();
    m_PassNodes.clear();
    m_PresentPassNode = nullptr;
    m_Resources.clear();
    m_BlackBoard.clear();
    m_InnerResources.clear();
    m_OutterResources.clear();
    m_ExecuteQueue.clear();
    m_Taskflow.clear();
}

}  // namespace hitagi::gfx