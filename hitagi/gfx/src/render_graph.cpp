#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>
#include <hitagi/utils/exceptions.hpp>

#include <taskflow/taskflow.hpp>

namespace hitagi::gfx {
void RenderGraph::PresentPass(ResourceHandle tex, Texture* back_buffer) {
    assert(tex < m_ResourceNodes.size());

    auto present_node  = std::make_shared<PassNodeWithData<CopyCommandContext, void>>();
    present_node->name = "Present";
    present_node->Read(tex);
    present_node->executor = [&, tex, back_buffer](CopyCommandContext* context) {
        auto texture = std::static_pointer_cast<Texture>(m_Resources[m_ResourceNodes.at(tex).res_idx]);
        // if (back_buffer) {
        //     context->CopyTextureRegion(
        //         *texture,
        //         {0, 0, 0, texture->width, texture->height, 1},
        //         *back_buffer,
        //         {0, 0, 0});
        //     context->Present(*back_buffer);
        // } else {
        //     context->Present(*static_cast<Texture*>(m_ResourceNodes.at(tex).resource));
        // }
    };

    m_PresentPassNode = present_node;
}

auto RenderGraph::Import(std::string_view name, std::shared_ptr<Resource> res) -> ResourceHandle {
    if (m_BlackBoard.contains(std::pmr::string(name))) {
        auto iter = std::find_if(m_ResourceNodes.begin(),
                                 m_ResourceNodes.end(),
                                 [&](const auto& res_node) {
                                     return res == m_Resources[res_node.res_idx];
                                 });

        assert(iter != m_ResourceNodes.end());
        return std::distance(m_ResourceNodes.begin(), iter);
    }

    ResourceHandle handle  = m_ResourceNodes.size();
    std::size_t    res_idx = m_Resources.size();
    m_Resources.emplace_back(res);
    m_BlackBoard.emplace(std::pmr::string(name), res_idx);
    m_ResourceNodes.emplace_back(name, res_idx);
    return handle;
}

auto RenderGraph::CreateResource(ResourceDesc desc) -> ResourceHandle {
    // create new resource node
    ResourceHandle handle = m_ResourceNodes.size();

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

    return handle;
}

auto RenderGraph::RequestCommandContext(CommandType type) -> std::shared_ptr<CommandContext> {
    std::shared_ptr<CommandContext> context = nullptr;
    if (m_ContextPool[type].empty() ||
        !m_Device.GetCommandQueue(CommandType::Graphics)->IsFenceComplete(m_ContextPool[type].front()->fence_value)) {
        context = m_Device.CreateGraphicsContext();
    } else {
        context = std::move(m_ContextPool[type].front());
        m_ContextPool[type].pop_front();
    }
    return context;
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
            if (auto writer = m_ResourceNodes.at(read_res).writer; writer != nullptr) {
                task.succeed(create_task(writer));
            }
        }

        m_ExecuteQueue.emplace_back(node);

        return task;
    };

    create_task(m_PresentPassNode.get());

    return true;
}

void RenderGraph::Execute() {
    for (auto&& [desc, res_idx] : m_InnerResourcesDesc) {
        auto& res = m_Resources[res_idx];
        std::visit(
            utils::Overloaded{
                [&](const GpuBuffer::Desc& buffer_desc) {
                    res = m_Device.CreateBuffer(buffer_desc);
                },
                [&](const Texture::Desc& txture_desc) {
                    res = m_Device.CreateTexture(txture_desc);
                },
            },
            desc);
    }
    m_Executor.run(m_Taskflow).wait();

    std::pmr::vector<CommandContext*> contexts;
    CommandQueue*                     prev_queue = nullptr;

    auto left  = m_ExecuteQueue.begin();
    auto right = left;
    while (left != m_ExecuteQueue.end()) {
        if (right != m_ExecuteQueue.end() && (*left)->type == (*right)->type) {
            right = std::next(right);
            continue;
        }

        std::transform(left, right, std::back_inserter(contexts), [](const PassNode* node) {
            return node->context.get();
        });

        auto queue = m_Device.GetCommandQueue((*left)->type);
        if (prev_queue) queue->WaitForQueue(*prev_queue);
        queue->Submit(contexts);

        std::for_each(left, right, [this](PassNode* node) {
            m_ContextPool[node->type].emplace_back(std::move(node->context));
        });

        prev_queue = queue;
        left       = right;
        contexts.clear();
    }
}

void RenderGraph::Reset() {
    m_ResourceNodes.clear();
    m_PassNodes.clear();
    m_PresentPassNode = nullptr;
    m_Resources.clear();
    m_BlackBoard.clear();
    m_ExecuteQueue.clear();
    m_Taskflow.clear();
}

}  // namespace hitagi::gfx