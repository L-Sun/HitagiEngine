#include <hitagi/graphics/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <taskflow/taskflow.hpp>

namespace hitagi::gfx {
void RenderGraph::PresentPass(ResourceHandle tex, resource::Texture* back_buffer) {
    assert(tex < m_ResourceNodes.size());

    m_PresentPassNode.name = "Present";
    m_PresentPassNode.Read(tex);
    m_PresentPassNode.executor = [&, tex, back_buffer](IGraphicsCommandContext* context) {
        auto texture = static_cast<resource::Texture*>(m_ResourceNodes.at(tex).resource);

        if (back_buffer) {
            context->CopyTextureRegion(
                *texture,
                {0, 0, 0, texture->width, texture->height, 1},
                *back_buffer,
                {0, 0, 0});
            context->Present(*back_buffer);
        } else {
            context->Present(*static_cast<resource::Texture*>(m_ResourceNodes.at(tex).resource));
        }
        m_LastFence = context->Finish();
    };
}

ResourceHandle RenderGraph::Import(resource::Resource* res) {
    if (auto iter = std::find_if(m_ResourceNodes.begin(),
                                 m_ResourceNodes.end(),
                                 [res](const auto& res_node) {
                                     return res == res_node.resource;
                                 });
        iter != m_ResourceNodes.end()) {
        return std::distance(m_ResourceNodes.begin(), iter);
    }
    ResourceHandle handle = m_ResourceNodes.size();
    m_Resources.emplace_back(res);
    m_BlackBoard.emplace_back(res);
    m_ResourceNodes.emplace_back(res->name, res);
    return handle;
}

ResourceHandle RenderGraph::CreateResource(std::string_view name, ResourceType resource) {
    // create new resource node
    ResourceHandle handle = m_ResourceNodes.size();

    m_InnerResources.emplace_back(std::move(resource));

    std::visit(
        utils::Overloaded{
            [&](resource::Resource& res) {
                res.name = name;
                m_ResourceNodes.emplace_back(name, &res);
                m_Resources.emplace_back(&res);
            },
        },
        m_InnerResources.back());

    return handle;
}

bool RenderGraph::Compile() {
    if (!m_PresentPassNode.executor) return false;

    std::pmr::unordered_map<const PassNode*, tf::Task> task_map;

    // bottom-up build taskflow
    // TODO: reuse command context when some pass nodes are in the same path
    std::function<tf::Task(const PassNode&)> create_task = [&](const PassNode& node) -> tf::Task {
        if (task_map.contains(&node))
            return task_map.at(&node);

        auto task = m_Taskflow
                        .emplace([&]() {
                            auto context = m_Device.CreateGraphicsCommandContext(node.name);
                            node.executor(context.get());
                        })
                        .name(std::string(node.name));

        task_map.emplace(&node, task);

        for (auto read_res : node.reads) {
            if (auto writer = m_ResourceNodes.at(read_res).writer; writer != nullptr) {
                auto pre_task = create_task(*writer);
                task.succeed(pre_task);
            }
        }

        return task;
    };

    create_task(m_PresentPassNode);
    return true;
}

void RenderGraph::Execute() {
    // Prepare all transiant resource used among the frame graph
    // TODO prepare the remaining resource after pruning.
    for (auto&& res : m_InnerResources) {
        std::visit(
            utils::Overloaded{
                [&](resource::Texture& res) {
                    m_Device.InitTexture(res);
                },
                [&](gfx::ConstantBuffer& buffer) {
                    m_Device.InitConstantBuffer(buffer);
                }},
            res);
    }

    m_Executor.run(m_Taskflow).wait();

    for (auto res : m_Resources) {
        if (res->gpu_resource) res->gpu_resource->fence_value = m_LastFence;
    }
}

void RenderGraph::Reset() {
    m_Device.WaitFence(m_LastFence);
    for (auto& res : m_InnerResources) {
        std::visit(
            utils::Overloaded{
                [&](resource::Resource& res) {
                    m_Device.RetireResource(std::move(res.gpu_resource));
                },
            },
            res);
    }

    m_ResourceNodes.clear();
    m_PassNodes.clear();
    m_Resources.clear();
    m_InnerResources.clear();
    m_BlackBoard.clear();
    m_Taskflow.clear();
}

}  // namespace hitagi::gfx