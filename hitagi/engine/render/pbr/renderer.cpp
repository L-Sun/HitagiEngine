export module render:pbr_renderer;
import core;
import app;
import asset;
import ecs;
import gfx;
import math;
import utils;
import std;
import :execution;
export import :scene;

export namespace hitagi::render::pbr {
using namespace hitagi;

inline constexpr std::string_view kPbrForwardContract = "PBRForward";

struct PbrMaterialDesc {
    hitagi::math::Color                     base_color = hitagi::math::Color::White();
    std::shared_ptr<hitagi::asset::Texture> base_color_texture;
    float                                   metallic  = 0.0f;
    float                                   roughness = 0.5f;
    std::shared_ptr<hitagi::asset::Texture> metallic_roughness_texture;
    std::shared_ptr<hitagi::asset::Texture> normal_texture;
    float                                   occlusion = 1.0f;
    std::shared_ptr<hitagi::asset::Texture> occlusion_texture;
    hitagi::math::Color                     emissive_color = hitagi::math::Color::Black();
    std::shared_ptr<hitagi::asset::Texture> emissive_texture;
    float                                   opacity      = 1.0f;
    bool                                    alpha_cutout = false;
    float                                   alpha_cutoff = 0.5f;
};

auto CreatePbrMaterialParameters(const PbrMaterialDesc& desc) -> hitagi::asset::MaterialParameters;
auto PbrMaterialDefaults() -> asset::MaterialParameters;

// Forward pass shell `pbr_forward.slang`, specialized by material compilers.
auto PbrForwardPassTemplate() -> asset::MaterialPassTemplate;

using render::RenderView;
using render::RenderDrawItem;

struct RenderLight {
    math::vec3f position;
    math::Color color     = math::Color::White();
    float       intensity = 1.0f;
};

struct RenderFrame {
    RenderView                      view;
    std::span<const RenderDrawItem> draw_items;
    std::span<const RenderLight>    lights;
};

struct RenderRequest {
    RenderFrame       frame;
    rg::TextureHandle target;
};

struct RenderResult {
    rg::TextureHandle color;
    rg::TextureHandle depth;
};

// A concrete runtime renderer, also usable as a pass builder for caller-owned graphs.
class PbrRenderer final : public core::RuntimeModule {
public:
    explicit PbrRenderer(const hitagi::asset::ResourceLoadContext& load_context);
    // Window rendering: the application must outlive this renderer.
    PbrRenderer(const asset::ResourceLoadContext& load_context, const Application& app);

    // Adds passes without executing. Keep this renderer and borrowed resources
    // alive through execution, before the next Build() or Tick() on this instance.
    auto Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult;

    // Retains a snapshot for window rendering on every Tick; initially empty.
    // Requires the constructor taking Application. Calls are host-serialized.
    void SetFrame(const RenderFrame& frame);
    // Queues one offscreen render. Copies spans and retains the target until Tick.
    void Submit(const RenderFrame& frame, std::shared_ptr<gfx::Texture> target);
    void Tick() final;
    ~PbrRenderer() final { UnloadAllSubModules(); }
    auto GetRenderedFrameCount() const noexcept -> std::uint64_t { return m_RenderedFrames; }

private:
    auto CountRenderableSubMeshes(const RenderRequest& request) const -> std::size_t;

    hitagi::gfx::Device&                  m_Device;
    hitagi::asset::ResourceLoadContext    m_LoadContext;
    std::shared_ptr<hitagi::gfx::Sampler> m_Sampler;
    struct PendingFrame {
        RenderView                    view;
        std::vector<RenderDrawItem>   draw_items;
        std::vector<RenderLight>      lights;
        std::shared_ptr<gfx::Texture> target;
    };
    void ExecuteFrame(const PendingFrame& frame, const std::shared_ptr<gfx::SwapChain>& swap_chain = {});

    const Application*              m_App = nullptr;
    std::shared_ptr<gfx::SwapChain> m_SwapChain;
    PendingFrame                    m_WindowFrame;
    std::vector<PendingFrame>       m_PendingFrames;
    std::uint64_t                   m_RenderedFrames = 0;
    // Declared last: graph callbacks/resources retire before renderer state.
    // Created on demand; callers using only Build need no private graph.
    std::unique_ptr<rg::RenderGraph> m_Graph;
};

// Shader mirrors live in pbr_forward.slang.
struct PbrFrameConstant {
    math::vec4f camera_pos;
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::mat4f inv_view;
    math::mat4f inv_projection;
    math::mat4f inv_proj_view;
    math::vec4f light_position;
    math::vec4f light_pos_in_view;
    math::vec3f light_color;
    float       light_intensity = 1.0f;
};

struct PbrInstanceConstant {
    math::mat4f model;
};

struct PbrBindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle instance_constant;
    std::uint32_t       instance_index  = 0;
    std::uint32_t       instance_stride = 0;
    gfx::BindlessHandle material_data;
    gfx::BindlessHandle sampler;
};

}  // namespace hitagi::render::pbr

namespace hitagi::render::pbr {

PbrRenderer::PbrRenderer(const asset::ResourceLoadContext& load_context, const Application& app)
    : PbrRenderer(load_context) {
    m_App       = &app;
    m_SwapChain = gfx::SwapChain::Create(m_LoadContext.device, m_LoadContext.queues.Get(gfx::CommandType::Graphics),
                                         {.name = std::pmr::string(GetName()), .window = app.GetWindow()});
}

void PbrRenderer::SetFrame(const RenderFrame& frame) {
    if (!m_App) throw std::logic_error("SetFrame requires a renderer constructed with Application");
    m_WindowFrame = {
        .view       = frame.view,
        .draw_items = {frame.draw_items.begin(), frame.draw_items.end()},
        .lights     = {frame.lights.begin(), frame.lights.end()},
        .target     = m_WindowFrame.target,
    };
}

void PbrRenderer::Submit(const RenderFrame& frame, std::shared_ptr<gfx::Texture> target) {
    detail::ValidateOutput(target);
    m_PendingFrames.push_back({
        .view       = frame.view,
        .draw_items = {frame.draw_items.begin(), frame.draw_items.end()},
        .lights     = {frame.lights.begin(), frame.lights.end()},
        .target     = std::move(target),
    });
}

void PbrRenderer::Tick() {
    RuntimeModule::Tick();
    auto frames = std::exchange(m_PendingFrames, {});
    for (const auto& frame : frames) ExecuteFrame(frame);

    if (m_App && detail::PrepareWindowTarget(*m_App, m_LoadContext.device, m_LoadContext.queues,
                                             m_LoadContext.bindings, m_SwapChain, m_WindowFrame.target)) {
        ExecuteFrame(m_WindowFrame, m_SwapChain);
    }
}

void PbrRenderer::ExecuteFrame(const PendingFrame& frame, const std::shared_ptr<gfx::SwapChain>& swap_chain) {
    try {
        if (!m_Graph) {
            m_Graph = std::make_unique<rg::RenderGraph>(m_LoadContext.device, m_LoadContext.queues,
                                                        m_LoadContext.bindings, GetName());
        }
        const auto result = Build(*m_Graph, {
                                                .frame  = {.view = frame.view, .draw_items = frame.draw_items, .lights = frame.lights},
                                                .target = m_Graph->Import(frame.target),
                                            });
        detail::FinishFrame(*m_Graph, result.color, swap_chain);
        ++m_RenderedFrames;
    } catch (...) {
        // Retire callbacks before releasing the frame snapshot; later frames
        // can start with a fresh graph after a failed build or execution.
        m_Graph.reset();
        throw;
    }
}

struct PbrDrawRecord {
    asset::Mesh::SubMesh                                           sub_mesh;
    std::shared_ptr<asset::Material>                               material;
    core::Buffer                                                   material_data;
    rg::GPUBufferEdgeHandle                                        material_access;
    std::pmr::vector<std::pair<std::uint8_t, rg::GPUBufferHandle>> vertex_buffers;
    rg::GPUBufferHandle                                            indices;
    gfx::Format                                                    index_format = gfx::Format::R32_UINT;
    rg::GPUBufferHandle                                            material_data_handle;
    std::shared_ptr<gfx::RenderPipeline>                           pipeline;
    std::size_t                                                    instance_index = 0;
};

void UploadMaterialData(
    const asset::MaterialPass& material_pass,
    gfx::GPUBuffer&            material_data_buffer) {
    const auto& material_data = material_pass.material_data;
    if (material_data.Empty()) return;

    auto* const mapped_data = static_cast<std::byte*>(material_data_buffer.Map());
    std::memcpy(mapped_data, material_data.GetData(), material_data.GetDataSize());
    material_data_buffer.UnMap();
}

auto MakeFrameConstant(const RenderRequest& request) -> PbrFrameConstant {
    const auto light = request.frame.lights.empty()
                           ? RenderLight{.position = {3.0f, -4.0f, 4.0f}, .color = math::Color::White(), .intensity = 30.0f}
                           : request.frame.lights.front();

    PbrFrameConstant frame{
        .camera_pos      = math::vec4f(request.frame.view.camera_position, 1.0f),
        .view            = request.frame.view.view,
        .projection      = request.frame.view.projection,
        .proj_view       = request.frame.view.projection * request.frame.view.view,
        .inv_view        = math::inverse(request.frame.view.view),
        .inv_projection  = math::inverse(request.frame.view.projection),
        .inv_proj_view   = math::inverse(request.frame.view.projection * request.frame.view.view),
        .light_position  = math::vec4f(light.position, 1.0f),
        .light_color     = {light.color[0], light.color[1], light.color[2]},
        .light_intensity = light.intensity,
    };
    frame.light_pos_in_view = frame.view * frame.light_position;
    return frame;
}

auto CreateDepthTarget(rg::RenderGraph& graph, const gfx::TextureDesc& color_desc) -> rg::TextureHandle {
    return graph.Create(gfx::TextureDesc{
        .name        = "PbrDepth",
        .width       = color_desc.width,
        .height      = color_desc.height,
        .format      = gfx::Format::D32_FLOAT,
        .clear_value = gfx::ClearDepthStencil{.depth = 1.0f},
        .usages      = gfx::TextureUsageFlags::DepthStencil | gfx::TextureUsageFlags::SRV,
    });
}

auto IsRenderablePbrMaterial(const std::shared_ptr<asset::Material>& material) noexcept -> bool {
    if (!material) return false;
    const auto* pass = material->FindPass(kPbrForwardContract);
    return pass != nullptr && pass->pipeline != nullptr;
}

auto CreatePbrMaterialPipeline(const asset::ResourceLoadContext& load_context, const asset::MaterialPass& pass) -> std::shared_ptr<gfx::RenderPipeline> {
    if (!pass.pipeline) return nullptr;
    pass.pipeline->Load(load_context);
    return pass.pipeline->GetBuiltPipeline();
}

auto CreatePbrMaterialParameters(const PbrMaterialDesc& desc) -> hitagi::asset::MaterialParameters {
    return hitagi::asset::MaterialParameters{
        {.name = "base_color", .value = desc.base_color},
        {.name = "metallic", .value = desc.metallic},
        {.name = "roughness", .value = desc.roughness},
        {.name = "occlusion", .value = desc.occlusion},
        {.name = "emissive_color", .value = desc.emissive_color},
        {.name = "opacity", .value = desc.opacity},
        {.name = "alpha_cutout", .value = static_cast<std::uint32_t>(desc.alpha_cutout ? 1u : 0u)},
        {.name = "alpha_cutoff", .value = desc.alpha_cutoff},
        {.name = "base_color_texture", .value = desc.base_color_texture},
        {.name = "metallic_roughness_texture", .value = desc.metallic_roughness_texture},
        {.name = "normal_texture", .value = desc.normal_texture},
        {.name = "occlusion_texture", .value = desc.occlusion_texture},
        {.name = "emissive_texture", .value = desc.emissive_texture},
    };
}

auto PbrMaterialDefaults() -> asset::MaterialParameters {
    return CreatePbrMaterialParameters(PbrMaterialDesc{});
}

auto PbrForwardPassTemplate() -> asset::MaterialPassTemplate {
    return {
        .contract = std::pmr::string(kPbrForwardContract),
        .shell    = {.name = "pbr_forward"},
        .stages   = {{.type = gfx::ShaderType::Vertex, .entry = "VSMain"}, {.type = gfx::ShaderType::Pixel, .entry = "PSMain"}},
        .pipeline = {
            .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .depth_stencil_state = {
                .depth_test_enable  = true,
                .depth_write_enable = true,
                .depth_compare_op   = gfx::CompareOp::Less,
            },
            .render_format        = gfx::Format::R8G8B8A8_UNORM,
            .depth_stencil_format = gfx::Format::D32_FLOAT,
        },
    };
}

PbrRenderer::PbrRenderer(const asset::ResourceLoadContext& load_context)
    : RuntimeModule("PbrRenderer"),
      m_Device(load_context.device),
      m_LoadContext(load_context),
      m_Sampler(hitagi::gfx::Sampler::Create(
          m_Device,
          m_LoadContext.bindings,
          {
              .name          = "PbrSampler",
              .address_u     = gfx::AddressMode::Repeat,
              .address_v     = gfx::AddressMode::Repeat,
              .address_w     = gfx::AddressMode::Repeat,
              .mag_filter    = gfx::FilterMode::Linear,
              .min_filter    = gfx::FilterMode::Linear,
              .mipmap_filter = gfx::FilterMode::Linear,
          })) {}

auto PbrRenderer::CountRenderableSubMeshes(const RenderRequest& request) const -> std::size_t {
    std::size_t count = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        for (const auto& sub_mesh : item.SubMeshes()) {
            if (IsRenderablePbrMaterial(sub_mesh.material)) ++count;
        }
    }
    return count;
}

auto PbrRenderer::Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult {
    const auto color = request.target;
    if (!graph.IsValid(color)) return {};

    const auto color_desc = graph.GetResourceDesc(color);
    auto       depth      = CreateDepthTarget(graph, color_desc);
    const auto draw_count = CountRenderableSubMeshes(request);
    if (draw_count == 0) {
        rg::RenderPassBuilder builder(graph);
        builder.SetName("PbrClear");
        builder.SetRenderTarget(color, true);
        builder.SetDepthStencil(depth, true);
        builder.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        builder.Finish();
        return {.color = color, .depth = depth};
    }

    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        item.mesh->Load(m_LoadContext);
        if (item.deformed_vertices) {
            if (item.deformed_vertices->Size() != item.mesh->vertices->Size()) throw std::invalid_argument("Deformed vertex count mismatch");
            item.deformed_vertices->Load(m_LoadContext);
        }
        for (const auto& sub_mesh : item.SubMeshes()) {
            if (sub_mesh.material) sub_mesh.material->Load(m_LoadContext);
        }
    }

    auto frame_constant    = graph.Create({
        .name   = "PbrFrameConstant",
        .size   = utils::align(sizeof(PbrFrameConstant), gfx::GPUBuffer::GetStorageViewRequirements(graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto instance_constant = graph.Create({
        .name   = "PbrInstanceConstant",
        .size   = sizeof(PbrInstanceConstant) * std::max<std::size_t>(1, request.frame.draw_items.size()),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto bindless_info     = graph.Create({
        .name   = "PbrBindlessInfo",
        .size   = sizeof(PbrBindlessInfo) * draw_count,
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });

    std::pmr::vector<PbrDrawRecord> draw_records;
    draw_records.reserve(draw_count);
    std::size_t instance_index = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) {
            ++instance_index;
            continue;
        }
        for (const auto& sub_mesh : item.SubMeshes()) {
            if (!IsRenderablePbrMaterial(sub_mesh.material)) continue;

            sub_mesh.material->Load(m_LoadContext);
            const auto* material_pass = sub_mesh.material->FindPass(kPbrForwardContract);
            if (!material_pass) continue;

            auto material_pipeline = CreatePbrMaterialPipeline(m_LoadContext, *material_pass);
            if (!material_pipeline) continue;

            auto material_data = material_pass->material_data;

            auto material_data_handle = graph.Create(
                {
                    .name   = std::pmr::string(std::format("PbrMaterialData-{}", draw_records.size())),
                    .size   = utils::align(material_data.GetDataSize(), gfx::GPUBuffer::GetStorageViewRequirements(graph.GetDevice()).size_alignment),
                    .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
                });

            draw_records.emplace_back(PbrDrawRecord{
                .sub_mesh             = sub_mesh,
                .material             = sub_mesh.material,
                .material_data        = std::move(material_data),
                .material_data_handle = material_data_handle,
                .pipeline             = std::move(material_pipeline),
                .instance_index       = instance_index,
            });
            auto& record = draw_records.back();

            for (const auto& vertex_attr : record.pipeline->GetDesc().vertex_input_layout) {
                const auto mesh_attr = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                auto       attr      = item.mesh->vertices->GetAttributeData(mesh_attr);
                if (item.deformed_vertices) {
                    if (auto deformed = item.deformed_vertices->GetAttributeData(mesh_attr)) attr = deformed;
                }
                if (attr && attr->get().gpu_buffer) {
                    record.vertex_buffers.emplace_back(vertex_attr.binding, graph.Import(attr->get().gpu_buffer));
                }
            }
            record.indices      = graph.Import(item.mesh->indices->GetGPUData());
            record.index_format = item.mesh->indices->Type() == asset::IndexType::UINT16 ? gfx::Format::R16_UINT : gfx::Format::R32_UINT;
        }
        ++instance_index;
    }

    auto                  sampler = graph.Import(m_Sampler);
    rg::RenderPassBuilder builder(graph);
    builder.SetName(std::format("PbrForwardPass-{}", graph.GetFrameIndex()));
    builder.SetRenderTarget(color, true);
    builder.SetDepthStencil(depth, true);
    const auto frame_constant_access = builder.Read(
        frame_constant,
        {
            .offset        = 0,
            .element_size  = sizeof(PbrFrameConstant),
            .element_count = 1,
        },
        gfx::PipelineStage::VertexShader | gfx::PipelineStage::PixelShader);
    const auto instance_constant_access = builder.Read(
        instance_constant,
        {
            .offset        = 0,
            .element_size  = sizeof(PbrInstanceConstant),
            .element_count = std::max<std::size_t>(1, request.frame.draw_items.size()),
        },
        gfx::PipelineStage::VertexShader);
    const auto bindless_info_access = builder.Read(
        bindless_info,
        {
            .offset        = 0,
            .element_size  = sizeof(PbrBindlessInfo),
            .element_count = draw_count,
        },
        gfx::PipelineStage::All);
    builder.AddSampler(sampler);

    for (auto& record : draw_records) {
        record.material_access = builder.Read(record.material_data_handle);
        for (const auto [_, vertex_buffer] : record.vertex_buffers) {
            builder.ReadAsVertices(vertex_buffer);
        }
        builder.ReadAsIndices(record.indices);
    }

    std::pmr::vector<math::mat4f> instance_transforms;
    instance_transforms.reserve(request.frame.draw_items.size());
    for (const auto& item : request.frame.draw_items) instance_transforms.emplace_back(item.transform);

    builder.SetExecutor([=, this, records = std::move(draw_records), transforms = std::move(instance_transforms), frame_data = MakeFrameConstant(request)](const rg::RenderGraph&, const rg::RenderPassNode& pass) mutable {
        pass.Resolve(frame_constant_access).GetMappedSpan<PbrFrameConstant>().front() = frame_data;

        auto instance_constants = pass.Resolve(instance_constant_access).GetMappedSpan<PbrInstanceConstant>();
        for (std::size_t i = 0; i < transforms.size(); ++i) {
            instance_constants[i] = {.model = transforms[i]};
        }

        auto        bindless_infos = pass.Resolve(bindless_info_access).GetMappedSpan<PbrBindlessInfo>();
        auto&       cmd            = pass.GetCmd();
        const auto& target         = pass.Resolve(color);
        cmd.SetViewPort({
            .x      = 0.0f,
            .y      = 0.0f,
            .width  = static_cast<float>(target.GetDesc().width),
            .height = static_cast<float>(target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = target.GetDesc().width,
            .height = target.GetDesc().height,
        });
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& record               = records[i];
            auto&       material_data_buffer = pass.Resolve(record.material_data_handle);
            const auto* material_pass        = record.material ? record.material->FindPass(kPbrForwardContract) : nullptr;
            if (!material_pass) continue;
            UploadMaterialData(
                *material_pass,
                material_data_buffer);

            auto& bindless = bindless_infos[i];
            bindless       = {
                .frame_constant    = pass.Resolve(frame_constant_access).GetBindlessHandle(),
                .instance_constant = pass.Resolve(instance_constant_access).GetBindlessHandle(),
                .instance_index    = static_cast<std::uint32_t>(record.instance_index),
                .instance_stride   = static_cast<std::uint32_t>(pass.Resolve(instance_constant_access).GetDesc().element_stride),
                .material_data     = pass.Resolve(record.material_access).GetBindlessHandle(),
                .sampler           = pass.Resolve(sampler).GetBindlessHandle(),
            };

            cmd.PushBindlessMetaInfo({
                .handle        = pass.Resolve(bindless_info_access).GetBindlessHandle(),
                .record_index  = static_cast<std::uint32_t>(i),
                .record_stride = static_cast<std::uint32_t>(pass.Resolve(bindless_info_access).GetDesc().element_stride),
            });

            cmd.SetPipeline(*record.pipeline);
            for (const auto [binding, vertex_buffer] : record.vertex_buffers) {
                cmd.SetVertexBuffers(binding, {{pass.Resolve(vertex_buffer)}}, {{0}});
            }
            cmd.SetIndexBuffer(pass.Resolve(record.indices), 0, record.index_format);
            cmd.DrawIndexed(record.sub_mesh.index_count, 1, record.sub_mesh.index_offset, record.sub_mesh.vertex_offset);
        }
    });
    builder.Finish();

    return {.color = color, .depth = depth};
}

}  // namespace hitagi::render::pbr
