export module render:toon_renderer;
import core;
import asset;
import ecs;
import gfx;
import math;
import utils;
import std;
import :execution;
export import :scene;

export namespace hitagi::render::toon {
using namespace hitagi;

inline constexpr std::string_view kToonForwardContract = "ToonForward";

struct ToonMaterialDesc {
    math::Color                     base_color = math::Color::White();
    std::shared_ptr<asset::Texture> base_color_texture;
    math::Color                     shadow_color       = math::Color(0.63f, 0.65f, 0.78f, 1.0f);
    math::Color                     outline_color      = math::Color(0.055f, 0.045f, 0.085f, 1.0f);
    float                           shadow_threshold   = 0.1f;
    float                           light_threshold    = 0.65f;
    float                           band_softness      = 0.035f;
    float                           specular_threshold = 0.96f;
    float                           specular_strength  = 0.08f;
    float                           rim_strength       = 0.06f;
    float                           outline_width      = 1.0f;  // Pixels at the output resolution.
    float                           opacity            = 1.0f;
    bool                            alpha_cutout       = false;
    float                           alpha_cutoff       = 0.5f;
};
inline constexpr std::string_view kToonOutlineContract = "ToonOutline";
auto                              CreateToonMaterialParameters(const ToonMaterialDesc& desc) -> asset::MaterialParameters;
auto                              ToonMaterialDefaults() -> asset::MaterialParameters;
auto                              ToonForwardPassTemplate() -> asset::MaterialPassTemplate;
auto                              ToonOutlinePassTemplate() -> asset::MaterialPassTemplate;
// Stock runtime materials need no editor/compiler dependency. Share these passes
// across materials, then supply independent parameter values.
auto CreateToonMaterialPasses() -> std::pmr::vector<asset::MaterialPass>;
auto CreateToonMaterial(const ToonMaterialDesc& desc = {}, std::string_view name = "Toon") -> std::shared_ptr<asset::Material>;

using render::RenderDrawItem;
using render::RenderView;

struct DirectionalLight {
    math::vec3f direction_to_light = {-0.4f, 0.7f, 1.0f};
    math::Color color              = math::Color::White();
    float       intensity          = 1.0f;
};
struct RenderFrame {
    RenderView                      view;
    std::span<const RenderDrawItem> draw_items;
    DirectionalLight                light;
};

struct RenderRequest {
    RenderFrame       frame;
    rg::TextureHandle target;
};

struct RenderResult {
    rg::TextureHandle color;
    rg::TextureHandle depth;
};

struct ToonSettings {
    // Render at twice the width/height and resolve color/depth at output size.
    bool supersampling = true;
};

// Builds rendering passes; RenderContext owns execution and presentation.
class ToonRenderer final {
public:
    explicit ToonRenderer(const hitagi::asset::ResourceLoadContext& load_context, ToonSettings settings = {});

    // Adds passes without executing. Keep this renderer and borrowed resources
    // alive through execution, before the next Build() on this instance.
    auto Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult;

    // Copies frame arrays; the returned builder borrows this renderer.
    auto Capture(const RenderFrame& frame) -> FrameBuilder;
    ~ToonRenderer() = default;

private:
    auto CountRenderableSubMeshes(const RenderRequest& request, std::string_view contract) const -> std::size_t;
    auto BuildPass(rg::RenderGraph& graph, const RenderRequest& request, rg::TextureHandle depth, std::string_view contract, bool clear) -> RenderResult;
    auto Resolve(rg::RenderGraph& graph, const RenderResult& source, rg::TextureHandle target) -> RenderResult;

    hitagi::gfx::Device&                  m_Device;
    hitagi::asset::ResourceLoadContext    m_LoadContext;
    std::shared_ptr<hitagi::gfx::Sampler> m_Sampler;
    ToonSettings                          m_Settings;
    std::shared_ptr<gfx::RenderPipeline>  m_ResolvePipeline;
};

// Shader mirrors live in toon_forward.slang.
struct ToonFrameConstant {
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::vec4f light_direction;  // View-space direction toward the light.
    math::vec4f light_color;      // RGB plus intensity.
    math::vec4f viewport;         // Width, height, inverse width, inverse height.
};
struct ToonInstanceConstant {
    math::mat4f model;
    math::mat4f normal_matrix;
};

struct ToonBindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle instance_constant;
    std::uint32_t       instance_index  = 0;
    std::uint32_t       instance_stride = 0;
    gfx::BindlessHandle material_data;
    gfx::BindlessHandle sampler;
};

}  // namespace hitagi::render::toon

namespace hitagi::render::toon {

auto ToonRenderer::Capture(const RenderFrame& frame) -> FrameBuilder {
    return [this, view = frame.view, draw_items = std::pmr::vector<RenderDrawItem>(frame.draw_items.begin(), frame.draw_items.end()), light = frame.light](rg::RenderGraph& graph, rg::TextureHandle target) {
        return Build(graph, {.frame = {.view = view, .draw_items = draw_items, .light = light}, .target = target}).color;
    };
}

struct ToonDrawRecord {
    std::pmr::vector<std::shared_ptr<gfx::TextureView>>            texture_views;
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
    const core::Buffer& material_data,
    gfx::GPUBuffer&     material_data_buffer) {
    if (material_data.Empty()) return;

    auto* const mapped_data = static_cast<std::byte*>(material_data_buffer.Map());
    std::memcpy(mapped_data, material_data.GetData(), material_data.GetDataSize());
    material_data_buffer.UnMap();
}

auto MakeFrameConstant(const RenderRequest& request, const gfx::TextureDesc& target, bool supersampling) -> ToonFrameConstant {
    const auto& view      = request.frame.view;
    const auto& light     = request.frame.light;
    const auto  direction = light.direction_to_light.norm() > 1e-6f
                                ? math::normalize(light.direction_to_light)
                                : math::vec3f(0.0f, 1.0f, 0.0f);
    return {.view = view.view, .projection = view.projection, .proj_view = view.projection * view.view, .light_direction = view.view * math::vec4f(direction, 0.0f), .light_color = {light.color[0], light.color[1], light.color[2], std::max(0.0f, light.intensity)}, .viewport = {float(target.width), float(target.height), (supersampling ? 2.0f : 1.0f) / target.width, (supersampling ? 2.0f : 1.0f) / target.height}};
}

auto CreateDepthTarget(rg::RenderGraph& graph, const gfx::TextureDesc& color_desc) -> rg::TextureHandle {
    return graph.Create(gfx::TextureDesc{
        .name        = "ToonDepth",
        .width       = color_desc.width,
        .height      = color_desc.height,
        .format      = gfx::Format::D32_FLOAT,
        .clear_value = gfx::ClearDepthStencil{.depth = 1.0f},
        .usages      = gfx::TextureUsageFlags::DepthStencil | gfx::TextureUsageFlags::SRV,
    });
}

auto IsRenderableToonMaterial(const std::shared_ptr<asset::Material>& material, std::string_view contract) noexcept -> bool {
    if (!material) return false;
    const auto* pass = material->FindPass(contract);
    return pass != nullptr && pass->pipeline != nullptr;
}

auto CreateToonMaterialPipeline(const asset::ResourceLoadContext& load_context, const asset::MaterialPass& pass) -> std::shared_ptr<gfx::RenderPipeline> {
    if (!pass.pipeline) return nullptr;
    pass.pipeline->Load(load_context);
    return pass.pipeline->GetBuiltPipeline();
}

auto CreateToonMaterialParameters(const ToonMaterialDesc& desc) -> asset::MaterialParameters {
    return {{"base_color", desc.base_color}, {"base_color_texture", desc.base_color_texture}, {"shadow_color", desc.shadow_color}, {"outline_color", desc.outline_color}, {"shadow_threshold", desc.shadow_threshold}, {"light_threshold", desc.light_threshold}, {"band_softness", desc.band_softness}, {"specular_threshold", desc.specular_threshold}, {"specular_strength", desc.specular_strength}, {"rim_strength", desc.rim_strength}, {"outline_width", desc.outline_width}, {"opacity", desc.opacity}, {"alpha_cutout", static_cast<std::uint32_t>(desc.alpha_cutout)}, {"alpha_cutoff", desc.alpha_cutoff}};
}

auto ToonMaterialDefaults() -> asset::MaterialParameters { return CreateToonMaterialParameters({}); }

auto ToonForwardPassTemplate() -> asset::MaterialPassTemplate {
    return {.contract = std::pmr::string(kToonForwardContract),
            .shell    = {.name = "toon_forward"},
            .stages   = {{.type = gfx::ShaderType::Vertex, .entry = "VSMain"},
                         {.type = gfx::ShaderType::Pixel, .entry = "PSMain"}},
            .pipeline = {.assembly_state       = {.primitive = gfx::PrimitiveTopology::TriangleList},
                         .rasterization_state  = {.cull_mode = gfx::CullMode::None},
                         .depth_stencil_state  = {.depth_test_enable = true, .depth_write_enable = true, .depth_compare_op = gfx::CompareOp::Less},
                         .render_format        = gfx::Format::R8G8B8A8_UNORM,
                         .depth_stencil_format = gfx::Format::D32_FLOAT}};
}

auto ToonOutlinePassTemplate() -> asset::MaterialPassTemplate {
    auto pass                                            = ToonForwardPassTemplate();
    pass.contract                                        = kToonOutlineContract;
    pass.stages[0].entry                                 = "VSOutline";
    pass.stages[1].entry                                 = "PSOutline";
    pass.pipeline.rasterization_state.cull_mode          = gfx::CullMode::Front;
    pass.pipeline.depth_stencil_state.depth_write_enable = false;
    return pass;
}

auto CreateToonMaterialPasses() -> std::pmr::vector<asset::MaterialPass> {
    std::pmr::vector<asset::MaterialBinding> bindings;
    std::string                              assignments;
    for (const auto& parameter : ToonMaterialDefaults()) {
        bindings.emplace_back(parameter.name, parameter.value);
        if (parameter.name != "base_color_texture")
            assignments += std::format("surface.{0} = data.{0};\n", parameter.name);
    }
    const auto                  fields = asset::MaterialDataFields(bindings);
    const auto                  layout = gfx::GenerateShaderStruct("MaterialData", fields, fields.back().offset + fields.back().size);
    const gfx::ShaderModuleDesc module{
        .name        = "toon_material",
        .source_code = std::pmr::string(std::format(R"(
import bindless;
import toon_forward;
{}
struct Material : IMaterial {{
    static void evaluate(SurfaceInput input, MaterialContext context, inout Surface surface) {{
        const MaterialData data = context.material_data.load<MaterialData>();
        {}
        if (hitagi::valid(data.base_color_texture.handle)) {{
            // Select at compile time: a runtime flag leaves fragment-only
            // implicit LOD instructions reachable from VSOutline in Debug SPIR-V.
            __stage_switch {{
                case vertex:
                    surface.base_color *= data.base_color_texture.sample_level<float4>(context.sampler, input.uv, 0.0);
                case fragment:
                    surface.base_color *= data.base_color_texture.sample<float4>(context.sampler, input.uv);
            }}
        }}
    }}
}};
)",
                                                    layout, assignments))};
    std::pmr::vector<asset::MaterialPass> passes;
    for (auto pass_template : {ToonForwardPassTemplate(), ToonOutlinePassTemplate()}) {
        std::pmr::vector<std::shared_ptr<asset::Shader>> shaders;
        for (const auto& stage : pass_template.stages) {
            shaders.emplace_back(std::make_shared<asset::Shader>(gfx::ShaderDesc{
                .name                 = std::pmr::string(std::format("Toon.{}", stage.entry)),
                .type                 = stage.type,
                .entry                = stage.entry,
                .program              = {.modules = {module, pass_template.shell}},
                .specialization_types = {"Material"}}));
        }
        passes.push_back({.pass_contract = pass_template.contract,
                          .pipeline      = std::make_shared<asset::RenderPipeline>(pass_template.pipeline, std::move(shaders)),
                          .bindings      = bindings});
    }
    return passes;
}

auto CreateToonMaterial(const ToonMaterialDesc& desc, std::string_view name) -> std::shared_ptr<asset::Material> {
    return std::make_shared<asset::Material>(CreateToonMaterialParameters(desc), CreateToonMaterialPasses(), name);
}

ToonRenderer::ToonRenderer(const asset::ResourceLoadContext& load_context, ToonSettings settings)
    : m_Device(load_context.device), m_LoadContext(load_context), m_Sampler(hitagi::gfx::Sampler::Create(m_Device, m_LoadContext.bindings, {
                                                                                                                                               .name          = "ToonSampler",
                                                                                                                                               .address_u     = gfx::AddressMode::Repeat,
                                                                                                                                               .address_v     = gfx::AddressMode::Repeat,
                                                                                                                                               .address_w     = gfx::AddressMode::Repeat,
                                                                                                                                               .mag_filter    = gfx::FilterMode::Linear,
                                                                                                                                               .min_filter    = gfx::FilterMode::Linear,
                                                                                                                                               .mipmap_filter = gfx::FilterMode::Linear,
                                                                                                                                           })),
      m_Settings(settings) {}

auto ToonRenderer::CountRenderableSubMeshes(const RenderRequest& request, std::string_view contract) const -> std::size_t {
    std::size_t count = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        for (const auto& sub_mesh : item.SubMeshes()) {
            if (IsRenderableToonMaterial(sub_mesh.material, contract)) ++count;
        }
    }
    return count;
}

auto ToonRenderer::Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult {
    if (!graph.IsValid(request.target)) return {};
    auto target_desc = graph.GetResourceDesc(request.target);
    if (target_desc.format != gfx::Format::R8G8B8A8_UNORM)
        throw std::invalid_argument("Toon requires an R8G8B8A8_UNORM output");
    auto raster_target = request.target;
    if (m_Settings.supersampling) {
        target_desc.name = "ToonSupersampledColor";
        target_desc.width *= 2;
        target_desc.height *= 2;
        target_desc.usages = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV;
        raster_target      = graph.Create(target_desc);
    }
    auto depth  = CreateDepthTarget(graph, target_desc);
    auto result = BuildPass(graph, {.frame = request.frame, .target = raster_target}, depth, kToonForwardContract, true);
    if (CountRenderableSubMeshes(request, kToonOutlineContract)) {
        result = BuildPass(graph, {.frame = request.frame, .target = graph.MoveFrom(result.color)},
                           graph.MoveFrom(result.depth), kToonOutlineContract, false);
    }
    return m_Settings.supersampling ? Resolve(graph, result, request.target) : result;
}

auto ToonRenderer::BuildPass(rg::RenderGraph& graph, const RenderRequest& request, rg::TextureHandle depth,
                             std::string_view contract, bool clear) -> RenderResult {
    const auto color = request.target;
    if (!graph.IsValid(color)) return {};

    const auto color_desc = graph.GetResourceDesc(color);
    const auto draw_count = CountRenderableSubMeshes(request, contract);
    if (draw_count == 0) {
        rg::RenderPassBuilder builder(graph);
        builder.SetName("ToonClear");
        builder.SetRenderTarget(color, clear);
        builder.SetDepthStencil(depth, clear);
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
        .name   = "ToonFrameConstant",
        .size   = utils::align(sizeof(ToonFrameConstant), gfx::GPUBuffer::GetStorageViewRequirements(graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto instance_constant = graph.Create({
        .name   = "ToonInstanceConstant",
        .size   = sizeof(ToonInstanceConstant) * std::max<std::size_t>(1, request.frame.draw_items.size()),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto bindless_info     = graph.Create({
        .name   = "ToonBindlessInfo",
        .size   = sizeof(ToonBindlessInfo) * draw_count,
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });

    std::pmr::vector<ToonDrawRecord> draw_records;
    draw_records.reserve(draw_count);
    std::size_t instance_index = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) {
            ++instance_index;
            continue;
        }
        for (const auto& sub_mesh : item.SubMeshes()) {
            if (!IsRenderableToonMaterial(sub_mesh.material, contract)) continue;

            sub_mesh.material->Load(m_LoadContext);
            const auto* material_pass = sub_mesh.material->FindPass(contract);
            if (!material_pass) continue;

            auto material_pipeline = CreateToonMaterialPipeline(m_LoadContext, *material_pass);
            if (!material_pipeline) continue;

            auto material_data = material_pass->material_data;

            auto material_data_handle = graph.Create(
                {
                    .name   = std::pmr::string(std::format("ToonMaterialData-{}", draw_records.size())),
                    .size   = utils::align(material_data.GetDataSize(), gfx::GPUBuffer::GetStorageViewRequirements(graph.GetDevice()).size_alignment),
                    .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
                });

            draw_records.emplace_back(ToonDrawRecord{
                .sub_mesh             = sub_mesh,
                .material             = sub_mesh.material,
                .material_data        = std::move(material_data),
                .material_data_handle = material_data_handle,
                .pipeline             = std::move(material_pipeline),
                .instance_index       = instance_index,
            });
            auto& record = draw_records.back();
            // Keep the exact externally allocated views whose handles were packed
            // into material_data, and declare their shader access to RenderGraph.
            const auto parameters = record.material->GetParameters();
            for (const auto& binding : material_pass->bindings) {
                const auto  parameter = std::ranges::find(parameters, binding.name, &asset::MaterialParameter::name);
                const auto& value     = parameter != parameters.end() && parameter->value.index() == binding.default_value.index()
                                            ? parameter->value
                                            : binding.default_value;
                const auto* texture   = std::get_if<std::shared_ptr<asset::Texture>>(&value);
                if (!texture || !*texture) continue;
                auto view = (*texture)->GetGPUView();
                if (!view && !(*texture)->Empty()) view = asset::Texture::DefaultTexture()->GetGPUView();
                if (view) record.texture_views.emplace_back(std::move(view));
            }

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
    builder.SetName(std::format("{}-{}", contract, graph.GetFrameIndex()));
    builder.SetRenderTarget(color, clear);
    builder.SetDepthStencil(depth, clear);
    const auto frame_constant_access = builder.Read(
        frame_constant,
        {
            .offset        = 0,
            .element_size  = sizeof(ToonFrameConstant),
            .element_count = 1,
        },
        gfx::PipelineStage::VertexShader | gfx::PipelineStage::PixelShader);
    const auto instance_constant_access = builder.Read(
        instance_constant,
        {
            .offset        = 0,
            .element_size  = sizeof(ToonInstanceConstant),
            .element_count = std::max<std::size_t>(1, request.frame.draw_items.size()),
        },
        gfx::PipelineStage::VertexShader);
    const auto bindless_info_access = builder.Read(
        bindless_info,
        {
            .offset        = 0,
            .element_size  = sizeof(ToonBindlessInfo),
            .element_count = draw_count,
        },
        gfx::PipelineStage::All);
    builder.AddSampler(sampler);

    for (auto& record : draw_records) {
        record.material_access = builder.Read(record.material_data_handle);
        for (const auto& view : record.texture_views)
            builder.Read(view, gfx::PipelineStage::VertexShader | gfx::PipelineStage::PixelShader);
        for (const auto [_, vertex_buffer] : record.vertex_buffers) {
            builder.ReadAsVertices(vertex_buffer);
        }
        builder.ReadAsIndices(record.indices);
    }

    std::pmr::vector<math::mat4f> instance_transforms;
    instance_transforms.reserve(request.frame.draw_items.size());
    for (const auto& item : request.frame.draw_items) instance_transforms.emplace_back(item.transform);

    builder.SetExecutor([=, this, records = std::move(draw_records), transforms = std::move(instance_transforms), frame_data = MakeFrameConstant(request, color_desc, m_Settings.supersampling)](const rg::RenderGraph&, const rg::RenderPassNode& pass) mutable {
        pass.Resolve(frame_constant_access).GetMappedSpan<ToonFrameConstant>().front() = frame_data;

        auto instance_constants = pass.Resolve(instance_constant_access).GetMappedSpan<ToonInstanceConstant>();
        for (std::size_t i = 0; i < transforms.size(); ++i) {
            instance_constants[i] = {.model = transforms[i], .normal_matrix = math::transpose(math::inverse(transforms[i]))};
        }

        auto        bindless_infos = pass.Resolve(bindless_info_access).GetMappedSpan<ToonBindlessInfo>();
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
            UploadMaterialData(
                record.material_data,
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

auto ToonRenderer::Resolve(rg::RenderGraph& graph, const RenderResult& source, rg::TextureHandle target) -> RenderResult {
    if (!m_ResolvePipeline) {
        std::pmr::vector<std::shared_ptr<gfx::Shader>> shaders;
        for (const auto type : {gfx::ShaderType::Vertex, gfx::ShaderType::Pixel}) {
            shaders.emplace_back(gfx::Shader::Create(m_Device, m_LoadContext.shader_compiler, {.name = "ToonResolve", .type = type, .entry = type == gfx::ShaderType::Vertex ? "VSMain" : "PSMain", .program = {.modules = {{.name = "toon_resolve"}}}}));
        }
        m_ResolvePipeline = gfx::RenderPipeline::Create(m_Device, m_LoadContext.bindings,
                                                        {.name = "ToonResolve", .rasterization_state = {.cull_mode = gfx::CullMode::None}, .depth_stencil_state = {.depth_test_enable = true, .depth_write_enable = true, .depth_compare_op = gfx::CompareOp::Always}, .render_format = gfx::Format::R8G8B8A8_UNORM, .depth_stencil_format = gfx::Format::D32_FLOAT}, shaders);
    }
    struct ResolveBindings {
        gfx::BindlessHandle color, depth;
    };
    const auto            depth  = CreateDepthTarget(graph, graph.GetResourceDesc(target));
    const auto            buffer = graph.Create({.name   = "ToonResolveBindings",
                                                 .size   = utils::align(sizeof(ResolveBindings), gfx::GPUBuffer::GetStorageViewRequirements(m_Device).size_alignment),
                                                 .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead});
    rg::RenderPassBuilder builder(graph);
    builder.SetName("ToonSupersamplingResolve");
    builder.SetRenderTarget(target, true);
    builder.SetDepthStencil(depth, true);
    const auto color_read  = builder.Read(source.color, {}, gfx::PipelineStage::PixelShader);
    const auto depth_read  = builder.Read(source.depth, {}, gfx::PipelineStage::PixelShader);
    const auto buffer_read = builder.Read(buffer, {.element_size = sizeof(ResolveBindings), .element_count = 1}, gfx::PipelineStage::PixelShader);
    builder.SetExecutor([=, pipeline = m_ResolvePipeline](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        pass.Resolve(buffer_read).GetMappedSpan<ResolveBindings>().front() = {
            pass.Resolve(color_read).GetBindlessHandle(), pass.Resolve(depth_read).GetBindlessHandle()};
        auto&       cmd  = pass.GetCmd();
        const auto& desc = pass.Resolve(target).GetDesc();
        cmd.SetViewPort({.width = float(desc.width), .height = float(desc.height)});
        cmd.SetScissorRect({.width = desc.width, .height = desc.height});
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({.handle = pass.Resolve(buffer_read).GetBindlessHandle()});
        cmd.Draw(3);
    });
    builder.Finish();
    return {.color = target, .depth = depth};
}

}  // namespace hitagi::render::toon
