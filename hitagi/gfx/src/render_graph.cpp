#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <taskflow/taskflow.hpp>

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
        context->Present(*helper.Get<Texture>(back_buffer));
    };

    m_PresentPassNode = present_pass;
}

auto RenderGraph::Import(std::string_view name, std::shared_ptr<Resource> res) -> ResourceHandle {
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
    m_InnerResourcesDesc.emplace_back(desc, res_idx);
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

bool RenderGraph::Compile() {
    if (m_PresentPassNode == nullptr) return false;

    std::pmr::unordered_map<const PassNode*, tf::Task> task_map;

    // bottom-up build taskflow
    // TODO: batch command context submit
    std::function<tf::Task(PassNode*)> create_task = [&](PassNode* node) -> tf::Task {
        if (task_map.contains(node))
            return task_map.at(node);

        node->context = RequestCommandContext(node->type);
        node->context->SetName(node->name);

        auto task = m_Taskflow.emplace([=]() {
            node->Execute();
            node->context->End();
        });

        task.name(std::string(node->name));
        task_map.emplace(node, task);

        for (auto read_res : node->reads) {
            if (auto writer = GetResrouceNode(read_res).writer; writer != nullptr) {
                task.succeed(create_task(writer));
            }
        }

        m_ExecuteQueue.emplace_back(node);

        return task;
    };

    create_task(m_PresentPassNode.get());

    for (auto&& [desc, res_idx] : m_InnerResourcesDesc) {
        auto& res = m_Resources[res_idx];
        std::visit(
            utils::Overloaded{
                [&](const GpuBuffer::Desc& buffer_desc) {
                    res = device.CreateBuffer(buffer_desc);
                },
                [&](const Texture::Desc& txture_desc) {
                    res = device.CreateTexture(txture_desc);
                },
            },
            desc);
    }

    return true;
}

void RenderGraph::Execute() {
    m_Executor.run(m_Taskflow).wait();

    std::pmr::vector<CommandContext*> contexts;
    std::uint64_t                     fence_value;

    auto left  = m_ExecuteQueue.begin();
    auto right = left;
    while (left != m_ExecuteQueue.end()) {
        if (right != m_ExecuteQueue.end() && (*left)->type == (*right)->type) {
            right = std::next(right);
            continue;
        }

        auto queue = device.GetCommandQueue((*left)->type);

        std::transform(left, right, std::back_inserter(contexts), [&](const PassNode* node) {
            for (auto res_handle : node->reads) {
                auto res = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                if (res->last_used_queue && res->last_used_queue != queue) {
                    queue->WaitForQueue(*res->last_used_queue);
                }
            }
            return node->context.get();
        });

        fence_value = queue->Submit(contexts);

        std::for_each(left, right, [&](PassNode* node) {
            m_ContextPool[node->type].emplace_back(std::move(node->context));
            for (auto res_handle : node->reads) {
                auto res             = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                res->fence_value     = fence_value;
                res->last_used_queue = queue;
            }
            for (auto res_handle : node->writes) {
                auto res             = m_Resources.at(GetResrouceNode(res_handle).res_idx);
                res->fence_value     = fence_value;
                res->last_used_queue = queue;
            }
        });

        left = right;
        contexts.clear();
    }
    Clear();
}

void RenderGraph::Clear() {
    for (auto& retired_resources : m_RetiredResources) {
        while (!retired_resources.empty()) {
            auto& retired_res = retired_resources.front();
            if (retired_res->last_used_queue->IsFenceComplete(retired_res->fence_value)) {
                retired_resources.pop_front();
            } else {
                break;
            }
        }
    }

    for (const auto& res : m_Resources) {
        if (std::find(m_RetiredResources[res->last_used_queue->type].begin(), m_RetiredResources[res->last_used_queue->type].end(), res) == m_RetiredResources[res->last_used_queue->type].end()) {
            m_RetiredResources[res->last_used_queue->type].emplace_back(res);
        }
    }
    for (auto& retired_resources : m_RetiredResources) {
        std::sort(retired_resources.begin(), retired_resources.end(), [](const auto& lhs, const auto& rhs) {
            return lhs->fence_value < rhs->fence_value;
        });
    }

    m_ResourceNodes.clear();
    m_PassNodes.clear();
    m_PresentPassNode = nullptr;
    m_Resources.clear();
    m_BlackBoard.clear();
    m_ExecuteQueue.clear();
    m_Taskflow.clear();
}

}  // namespace hitagi::gfx