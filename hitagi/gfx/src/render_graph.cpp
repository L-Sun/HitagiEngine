#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/overloaded.hpp>
#include <hitagi/utils/flags.hpp>

#include <fmt/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <range/v3/all.hpp>

#include <set>
#include "hitagi/gfx/common_types.hpp"

namespace hitagi::gfx {

RenderGraph::RenderGraph(Device& device, std::string_view name)
    : m_Device(device),
      m_Name(name),
      m_Logger(spdlog::stdout_color_mt(std::string(name)))

{
    m_Logger->trace("Create Render Graph: ({})", fmt::styled(m_Name.c_str(), fmt::fg(fmt::color::green)));

    magic_enum::enum_for_each<CommandType>([&](CommandType type) {
        m_Fences.at(type) = {
            .fence = m_Device.CreateFence(0, fmt::format("{}-{}Fence", name, magic_enum::enum_name(type))),
        };
    });
}

RenderGraph::~RenderGraph() {
    WaitLastFrame();
    ranges::for_each(m_CompiledExecuteLayers |
                         ranges::views::join | ranges::views::join |
                         ranges::views::transform([](auto pass_node) {
                             return ranges::views::concat(pass_node->m_ReadResourceHandles, pass_node->m_WriteResourceHandles);
                         }) |
                         ranges::views::join |
                         ranges::views::values |
                         ranges::views::transform([](auto access_info) {
                             return access_info.bindless_handle;
                         }),
                     [&](auto handle) {
                         m_Device.GetBindlessUtils().DiscardBindlessHandle(handle);
                     });
}

auto RenderGraph::Import(std::shared_ptr<Resource> resource) -> ResourceHandle {
    if (std::find(m_OuterResources.begin(), m_OuterResources.end(), resource) != m_OuterResources.end()) {
        m_Logger->warn("Resource already imported: ({})", fmt::styled(resource->GetName(), fmt::fg(fmt::color::yellow)));
        return {};
    }
    m_Logger->trace("Import Resource: ({})", fmt::styled(resource->GetName(), fmt::fg(fmt::color::green)));

    const auto& new_resource_node = m_ResourceNodes.emplace_back(ResourceNode{
        *this,
        ResourceHandle{m_ResourceNodes.size()},
        m_Resources.size(),
    });
    m_Resources.emplace_back(resource);
    m_OuterResources.emplace_back(std::move(resource));
    return new_resource_node.m_Handle;
}

auto RenderGraph::Create(ResourceDesc desc) -> ResourceHandle {
    const auto& new_resource_node = m_ResourceNodes.emplace_back(ResourceNode{
        *this,
        ResourceHandle{m_ResourceNodes.size()},
        m_Resources.size(),
    });
    m_Resources.emplace_back(desc);
    return new_resource_node.m_Handle;
}

void RenderGraph::AddPresentPass(ResourceHandle swap_chain_handle) {
    struct PresentPass {
    };
    AddPass<PresentPass, CommandType::Graphics>(
        "PresentPass",
        [&](auto& builder, auto&) {
            builder.Read(swap_chain_handle, PipelineStage::Render);
        },
        [=](const ResourceHelper& helper, const auto&, auto& context) {
            context.Present(helper.Get<SwapChain>(swap_chain_handle));
        });
}

void RenderGraph::Compile() {
    if (m_IsCompiled) {
        m_Logger->warn("Render Graph already compiled.");
        return;
    }
    if (!m_PassNodes.contains("PresentPass")) {
        m_Logger->error("Present Pass not found.");
        return;
    }

    std::shared_ptr<PassNodeBase> present_pass = m_PassNodes.at("PresentPass");

    m_CompiledExecuteLayers.emplace_back(ExecuteLayer{
        BatchPasses{present_pass.get()},
        BatchPasses{},
        BatchPasses{},
    });

    for (std::size_t current_layer = 0; current_layer < m_CompiledExecuteLayers.size(); current_layer++) {
        BatchPasses new_batch = m_CompiledExecuteLayers[current_layer] |
                                ranges::views::join |
                                ranges::views::transform(
                                    [](const auto pass_node) {
                                        return pass_node->GetInResourceHandles();
                                    }) |
                                ranges::views::join |
                                ranges::views::transform(
                                    [this](auto handle) {
                                        return const_cast<PassNodeBase*>(GetResourceNode(handle).m_Writer);
                                    }) |
                                ranges::views::filter([](auto writer) {
                                    return writer != nullptr;
                                }) |
                                ranges::to<BatchPasses>() |
                                ranges::actions::unique;
        if (new_batch.empty()) {
            break;
        }
        ExecuteLayer new_layer = {};
        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            new_layer[type] = new_batch |
                              ranges::views::filter([type](auto pass_node) {
                                  return pass_node->GetCommandType() == type;
                              }) |
                              ranges::to<BatchPasses>();
        });

        m_CompiledExecuteLayers.emplace_back(std::move(new_layer));
    }

    std::reverse(m_CompiledExecuteLayers.begin(), m_CompiledExecuteLayers.end());

    InitializeResource();

    m_IsCompiled = true;
}

void RenderGraph::Execute() {
    if (!m_IsCompiled) {
        m_Logger->error("Render Graph not compiled.");
        return;
    }

    WaitLastFrame();

    using BatchContexts = std::pmr::vector<CommandContext*>;

    utils::EnumArray<std::optional<FenceSignalInfo>, CommandType> last_signal_info;

    for (std::size_t layer_index = 0; layer_index < m_CompiledExecuteLayers.size(); layer_index++) {
        const auto& curr_execute_layer = m_CompiledExecuteLayers[layer_index];

        magic_enum::enum_for_each<CommandType>([&](CommandType type) {
            BatchContexts batch_contexts = curr_execute_layer[type] |
                                           ranges::views::transform([&](auto pass_node) {
                                               pass_node->m_CommandContext = m_Device.CreateCommandContext(type, pass_node->GetName());
                                               pass_node->Execute();
                                               return pass_node->m_CommandContext.get();
                                           }) |
                                           ranges::to<BatchContexts>();

            if (batch_contexts.empty()) return;

            utils::EnumArray<std::optional<FenceWaitInfo>, CommandType> wait_infos;
            if (layer_index != 0) {
                const auto& last_execute_layer = m_CompiledExecuteLayers[layer_index - 1];

                magic_enum::enum_for_each<CommandType>([&](CommandType last_layer_type) {
                    if (wait_infos[last_layer_type].has_value()) return;
                    auto pass_node_pairs = ranges::views::cartesian_product(
                        curr_execute_layer[type],
                        last_execute_layer[last_layer_type]);
                    for (auto [curr_layer_pass_node, last_layer_pass_node] : pass_node_pairs) {
                        if (curr_layer_pass_node->IsDependOn(*last_layer_pass_node) && last_signal_info[last_layer_type].has_value()) {
                            wait_infos[last_layer_type].emplace(FenceWaitInfo{
                                .fence = last_signal_info[last_layer_type].value().fence,
                                .value = last_signal_info[last_layer_type].value().value,
                            });
                            break;
                        }
                    }
                });
            }
            auto wait_fences = wait_infos |
                               ranges::views::filter([](auto wait_info) {
                                   return wait_info.has_value();
                               }) |
                               ranges::views::transform([](auto wait_info) {
                                   return *wait_info;
                               }) |
                               ranges::to<std::pmr::vector<FenceWaitInfo>>();

            auto& queue = m_Device.GetCommandQueue(type);

            queue.Submit(
                batch_contexts,
                wait_fences,
                {FenceSignalInfo{
                    .fence = *m_Fences[type].fence,
                    .value = m_Fences[type].next_value++,
                }});
        });
    }

    m_FrameIndex++;
}

auto RenderGraph::GetResourceNode(ResourceHandle handle) const -> const ResourceNode& {
    return m_ResourceNodes.at(handle.node_index);
}

auto RenderGraph::GetResourceNode(ResourceHandle handle) -> ResourceNode& {
    return m_ResourceNodes.at(handle.node_index);
}

auto RenderGraph::GetResource(ResourceHandle handle) const -> const DelayResource& {
    return m_Resources.at(GetResourceNode(handle).m_ResourceIndex);
}

auto RenderGraph::GetResourceName(ResourceHandle handle) const -> std::string_view {
    const auto& resource = GetResource(handle);
    return std::visit(utils::Overloaded{
                          [](const std::shared_ptr<Resource>& resource) { return resource->GetName(); },
                          [](const ResourceDesc& desc) { return std::visit([](const auto& _desc) { return _desc.name; }, desc); },
                      },
                      resource);
};

void RenderGraph::InitializeResource() {
    auto delay_resources_index = m_CompiledExecuteLayers |
                                 ranges::views::join | ranges::views::join |
                                 ranges::views::transform([&](const auto pass_node) {
                                     return pass_node->GetInResourceHandles();
                                 }) |
                                 ranges::views::join |
                                 ranges::views::transform([&](auto handle) -> std::size_t {
                                     return GetResourceNode(handle).m_ResourceIndex;
                                 }) |
                                 ranges::to<std::pmr::set<std::size_t>>();

    auto delay_resources = delay_resources_index |
                           ranges::views::transform([&](std::size_t index) -> DelayResource& {
                               return m_Resources[index];
                           });

    // Initialize resource
    for (DelayResource& resource : delay_resources) {
        if (std::holds_alternative<ResourceDesc>(resource)) {
            resource = std::visit(
                utils::Overloaded{
                    [this](const GPUBufferDesc& desc) -> std::shared_ptr<Resource> {
                        return m_Device.CreateGPUBuffer(desc);
                    },
                    [this](const TextureDesc& desc) -> std::shared_ptr<Resource> {
                        return m_Device.CreateTexture(desc);
                    },
                },
                std::get<ResourceDesc>(resource));
        }
    }

    // Initialize bindless
    auto pass_resource_infos = m_CompiledExecuteLayers |
                               ranges::views::join | ranges::views::join |
                               ranges::views::transform([&](auto pass_node) {
                                   return ranges::views::concat(pass_node->m_ReadResourceHandles, pass_node->m_WriteResourceHandles);
                               }) |
                               ranges::views::join;

    for (auto& [handle, access_info] : pass_resource_infos) {
        auto resource = std::get<std::shared_ptr<Resource>>(GetResource(handle));

        auto& bindless_utils = m_Device.GetBindlessUtils();
        switch (resource->GetType()) {
            case ResourceType::GPUBuffer: {
                auto buffer = std::static_pointer_cast<GPUBuffer>(resource);
                // TODO index
                if (utils::has_flag(buffer->GetDesc().usages, GPUBufferUsageFlags::Constant) ||
                    utils::has_flag(buffer->GetDesc().usages, GPUBufferUsageFlags::Storage)) {
                    access_info.bindless_handle = bindless_utils.CreateBindlessHandle(*buffer, access_info.write);
                }
            } break;
            case ResourceType::Texture: {
                auto texture                = std::static_pointer_cast<Texture>(resource);
                access_info.bindless_handle = bindless_utils.CreateBindlessHandle(*texture, access_info.write);
            } break;
            case ResourceType::Sampler: {
                auto sampler                = std::static_pointer_cast<Sampler>(resource);
                access_info.bindless_handle = bindless_utils.CreateBindlessHandle(*sampler);
            } break;
            default: {
            }
        }
    }
}

void RenderGraph::WaitLastFrame() {
    m_Logger->trace("Wait last frame {}", m_FrameIndex);
    for (const auto& fence_info : m_Fences) {
        fence_info.fence->Wait(fence_info.next_value - 1);
    }
}

ResourceNode::ResourceNode(const RenderGraph& render_graph, ResourceHandle resource_handle, std::size_t resource_index)
    : m_RenderGraph(render_graph), m_Handle(resource_handle), m_ResourceIndex(resource_index) {}

auto ResourceNode::GetResource() const noexcept -> const DelayResource& {
    return m_RenderGraph.GetResource(m_Handle);
}

auto ResourceNode::GetPrevVersion() const noexcept -> const ResourceNode& {
    return m_RenderGraph.GetResourceNode(m_PrevVersionHandle);
}

auto ResourceNode::GetNextVersion() const noexcept -> const ResourceNode& {
    return m_RenderGraph.GetResourceNode(m_NextVersionHandle);
}

auto PassNodeBase::GetInResourceHandles() const noexcept -> const std::pmr::vector<ResourceHandle> {
    auto read_handles      = m_ReadResourceHandles | ranges::views::keys;
    auto pre_write_handles = m_WriteResourceHandles |
                             ranges::views::keys |
                             ranges::views::transform([&](ResourceHandle handle) {
                                 return m_RenderGraph.GetResourceNode(handle).GetPrevVersion().GetHandle();
                             });
    return ranges::views::concat(read_handles, pre_write_handles) | ranges::to<std::pmr::vector<ResourceHandle>>();
}

bool PassNodeBase::IsDependOn(const PassNodeBase& other) const noexcept {
    for (auto in_handles : GetInResourceHandles()) {
        if (other.m_WriteResourceHandles.contains(in_handles)) {
            return true;
        }
    }
    return false;
}

void PassNodeBase::ResourceBarrier() {
    auto buffer_barriers = ranges::views::concat(m_ReadResourceHandles, m_WriteResourceHandles) |
                           ranges::views::filter([&](const auto& info) {
                               auto        handle   = info.first;
                               const auto& resource = m_RenderGraph.GetResource(handle);
                               return std::get<std::shared_ptr<Resource>>(resource)->GetType() == ResourceType::GPUBuffer;
                           }) |
                           ranges::views::transform([&](const auto& info) {
                               const auto& [handle, access_info] = info;
                               auto resource                     = std::get<std::shared_ptr<Resource>>(m_RenderGraph.GetResource(handle));
                               auto buffer                       = std::static_pointer_cast<GPUBuffer>(resource);
                               // TODO
                               auto dst_access = BarrierAccess::Unkown;
                               if (access_info.write) {
                                   dst_access |= BarrierAccess::ShaderWrite;
                               }
                               if (utils::has_flag(buffer->GetDesc().usages, GPUBufferUsageFlags::Constant)) {
                                   dst_access |= BarrierAccess::Constant;
                               }
                               if (utils::has_flag(buffer->GetDesc().usages, GPUBufferUsageFlags::Vertex)) {
                                   dst_access |= BarrierAccess::Vertex;
                               }
                               if (utils::has_flag(buffer->GetDesc().usages, GPUBufferUsageFlags::Index)) {
                                   dst_access |= BarrierAccess::Index;
                               }

                               return GPUBufferBarrier{
                                   .src_access = BarrierAccess::Unkown,
                                   .dst_access = dst_access,
                                   .src_stage  = PipelineStage::None,
                                   .dst_stage  = PipelineStage::All,
                                   .buffer     = *buffer,
                               };
                           }) |
                           ranges::to<std::pmr::vector<GPUBufferBarrier>>();

    auto texture_barriers = ranges::views::concat(m_ReadResourceHandles, m_WriteResourceHandles) |
                            ranges::views::filter([&](const auto& info) {
                                auto        handle   = info.first;
                                const auto& resource = m_RenderGraph.GetResource(handle);
                                return std::get<std::shared_ptr<Resource>>(resource)->GetType() == ResourceType::Texture;
                            }) |
                            ranges::views::transform([&](const auto& info) {
                                const auto& [handle, access_info] = info;
                                auto resource                     = std::get<std::shared_ptr<Resource>>(m_RenderGraph.GetResource(handle));
                                auto texture                      = std::static_pointer_cast<Texture>(resource);

                                TextureBarrier barrier{.texture = *texture};

                                // TODO
                                if (access_info.write && utils::has_flag(texture->GetDesc().usages, TextureUsageFlags::RTV)) {
                                    barrier.dst_access = BarrierAccess::RenderTarget;
                                    barrier.dst_layout = TextureLayout::RenderTarget;
                                    barrier.src_layout = TextureLayout::Common;
                                }
                                if (access_info.write && utils::has_flag(texture->GetDesc().usages, TextureUsageFlags::DSV)) {
                                    barrier.dst_access = BarrierAccess::DepthStencilRead | BarrierAccess::DepthStencilWrite;
                                    barrier.dst_layout = TextureLayout::DepthStencilWrite;
                                }
                                if (access_info.write && utils::has_flag(texture->GetDesc().usages, TextureUsageFlags::UAV)) {
                                    barrier.dst_access = BarrierAccess::ShaderRead | BarrierAccess::ShaderWrite;
                                    barrier.dst_layout = TextureLayout::ShaderWrite;
                                } else {
                                    barrier.dst_access |= BarrierAccess::ShaderRead;
                                    barrier.dst_layout = TextureLayout::ShaderRead;
                                }

                                return barrier;
                            }) |
                            ranges::to<std::pmr::vector<TextureBarrier>>();
    m_CommandContext->ResourceBarrier({}, buffer_barriers, texture_barriers);
}

auto PassBuilderBase::Read(ResourceHandle handle, PipelineStage stage) -> ResourceHandle {
    if (m_PassNode.m_WriteResourceHandles.contains(handle)) {
        m_RenderGraph.m_Logger->error(
            "Pass({}) can not read a resource that mark as write usage.",
            fmt::styled(m_PassNode.GetName(), fmt::fg(fmt::color::yellow)),
            fmt::styled(m_RenderGraph.GetResourceName(handle), fmt::fg(fmt::color::yellow)));
        return {};
    }
    if (m_PassNode.m_ReadResourceHandles.contains(handle)) {
        m_PassNode.m_ReadResourceHandles.at(handle).stage |= stage;
        return handle;
    }
    auto& read_resource_node = m_RenderGraph.GetResourceNode(handle);

    if (auto iter = std::find_if(
            m_PassNode.m_ReadResourceHandles.begin(), m_PassNode.m_ReadResourceHandles.end(),
            [&](const auto& in_resource_info) {
                auto        in_resource_handle = in_resource_info.first;
                const auto& in_resource_node   = m_RenderGraph.GetResourceNode(in_resource_handle);
                return read_resource_node.m_ResourceIndex == in_resource_node.m_ResourceIndex;
            });
        iter != m_PassNode.m_ReadResourceHandles.end()) {
        const auto& in_resource_node = m_RenderGraph.GetResourceNode(iter->first);

        m_RenderGraph.m_Logger->warn(
            "Pass {} can not read different version({}, {}) of same resource {}",
            fmt::styled(m_PassNode.GetName(), fmt::fg(fmt::color::yellow)),
            fmt::styled(in_resource_node.GetVersion(), fmt::fg(fmt::color::yellow)),
            fmt::styled(read_resource_node.GetVersion(), fmt::fg(fmt::color::yellow)),
            fmt::styled(m_RenderGraph.GetResourceName(handle), fmt::fg(fmt::color::yellow)));
        return {};
    }

    m_PassNode.m_ReadResourceHandles.emplace(handle, PassNodeBase::ResourceAccessInfo{.stage = stage, .write = false});
    read_resource_node.m_Readers.emplace(&m_PassNode);

    return read_resource_node.m_Handle;
}

auto PassBuilderBase::Write(ResourceHandle handle, PipelineStage stage) -> ResourceHandle {
    if (m_PassNode.m_ReadResourceHandles.contains(handle)) {
        m_RenderGraph.m_Logger->error(
            "Pass({}) can not write a resource that mark as read usage.",
            fmt::styled(m_PassNode.GetName(), fmt::fg(fmt::color::yellow)),
            fmt::styled(m_RenderGraph.GetResourceName(handle), fmt::fg(fmt::color::yellow)));
        return {};
    }
    if (m_PassNode.m_WriteResourceHandles.contains(handle)) {
        m_PassNode.m_WriteResourceHandles.at(handle).stage |= stage;
        return handle;
    }

    auto& write_resource_node = m_RenderGraph.GetResourceNode(handle);
    if (!write_resource_node.IsNewest()) {
        m_RenderGraph.m_Logger->warn(
            "Pass {} can not write old version({}) of resource {}",
            fmt::styled(m_PassNode.GetName(), fmt::fg(fmt::color::yellow)),
            fmt::styled(write_resource_node.GetVersion(), fmt::fg(fmt::color::yellow)),
            fmt::styled(m_RenderGraph.GetResourceName(handle), fmt::fg(fmt::color::yellow)));
        return {};
    }

    // Create new resource node
    auto& new_resource_node = m_RenderGraph.m_ResourceNodes.emplace_back(ResourceNode{
        m_RenderGraph,
        m_RenderGraph.m_ResourceNodes.size(),
        write_resource_node.m_ResourceIndex,
    });
    // the `write_resource_node` may be invalid after `emplace_back`
    auto& old_resource_node = m_RenderGraph.GetResourceNode(handle);

    new_resource_node.m_PrevVersionHandle = old_resource_node.m_Handle;
    old_resource_node.m_NextVersionHandle = new_resource_node.m_Handle;
    new_resource_node.m_Version           = old_resource_node.m_Version + 1;

    new_resource_node.m_Writer = &m_PassNode;
    m_PassNode.m_WriteResourceHandles.emplace(new_resource_node.m_Handle, PassNodeBase::ResourceAccessInfo{.stage = stage, .write = true});

    return new_resource_node.m_Handle;
}

auto ResourceHelper::Get(ResourceHandle handle) const -> BindlessHandle {
    if (m_RenderPass.m_ReadResourceHandles.contains(handle)) {
        return m_RenderPass.m_ReadResourceHandles.at(handle).bindless_handle;
    } else if (m_RenderPass.m_WriteResourceHandles.contains(handle)) {
        return m_RenderPass.m_WriteResourceHandles.at(handle).bindless_handle;
    } else {
        return {};
    }
}

}  // namespace hitagi::gfx