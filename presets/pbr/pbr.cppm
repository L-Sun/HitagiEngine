module;
#include <cstddef>

export module preset.pbr;
import engine;
import std;

export namespace hitagi::preset::pbr {
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

class PbrRenderer final : public hitagi::render::IRenderer {
public:
    explicit PbrRenderer(const hitagi::asset::ResourceLoadContext& load_context);

    auto Render(hitagi::render::RenderContext& context, const hitagi::render::RenderRequest& request) -> hitagi::render::RenderResult final;

private:
    auto CountRenderableSubMeshes(const hitagi::render::RenderRequest& request) const -> std::size_t;

    hitagi::gfx::Device&                  m_Device;
    hitagi::asset::ResourceLoadContext    m_LoadContext;
    std::shared_ptr<hitagi::gfx::Sampler> m_Sampler;
};

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

inline auto PbrFrameConstantShaderDeclaration() -> std::pmr::string {
    const std::array fields{
        gfx::MakeShaderField<decltype(PbrFrameConstant::camera_pos)>("camera_pos", offsetof(PbrFrameConstant, camera_pos)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::view)>("view", offsetof(PbrFrameConstant, view)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::projection)>("projection", offsetof(PbrFrameConstant, projection)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::proj_view)>("proj_view", offsetof(PbrFrameConstant, proj_view)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::inv_view)>("inv_view", offsetof(PbrFrameConstant, inv_view)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::inv_projection)>("inv_projection", offsetof(PbrFrameConstant, inv_projection)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::inv_proj_view)>("inv_proj_view", offsetof(PbrFrameConstant, inv_proj_view)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::light_position)>("light_position", offsetof(PbrFrameConstant, light_position)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::light_pos_in_view)>("light_pos_in_view", offsetof(PbrFrameConstant, light_pos_in_view)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::light_color)>("light_color", offsetof(PbrFrameConstant, light_color)),
        gfx::MakeShaderField<decltype(PbrFrameConstant::light_intensity)>("light_intensity", offsetof(PbrFrameConstant, light_intensity)),
    };
    return gfx::GenerateShaderStruct("FrameConstant", fields, sizeof(PbrFrameConstant));
}

inline auto PbrInstanceConstantShaderDeclaration() -> std::pmr::string {
    const std::array fields{
        gfx::MakeShaderField<decltype(PbrInstanceConstant::model)>("model", offsetof(PbrInstanceConstant, model)),
    };
    return gfx::GenerateShaderStruct("InstanceConstant", fields, sizeof(PbrInstanceConstant));
}

inline auto PbrBindlessInfoShaderDeclaration() -> std::pmr::string {
    const std::array fields{
        gfx::MakeShaderResourceField("frame_constant", offsetof(PbrBindlessInfo, frame_constant), gfx::BindlessHandleType::Buffer),
        gfx::MakeShaderResourceField("instance_constant", offsetof(PbrBindlessInfo, instance_constant), gfx::BindlessHandleType::Buffer),
        gfx::MakeShaderField<decltype(PbrBindlessInfo::instance_index)>("instance_index", offsetof(PbrBindlessInfo, instance_index)),
        gfx::MakeShaderField<decltype(PbrBindlessInfo::instance_stride)>("instance_stride", offsetof(PbrBindlessInfo, instance_stride)),
        gfx::MakeShaderResourceField("material_data", offsetof(PbrBindlessInfo, material_data), gfx::BindlessHandleType::Buffer),
        gfx::MakeShaderResourceField("sampler", offsetof(PbrBindlessInfo, sampler), gfx::BindlessHandleType::Sampler),
    };
    return gfx::GenerateShaderStruct("BindlessInfo", fields, sizeof(PbrBindlessInfo));
}

auto PbrMaterialDefaults() -> asset::MaterialParameters;

}  // namespace hitagi::preset::pbr

namespace hitagi::preset::pbr {
struct PbrDrawRecord {
    asset::Mesh::SubMesh                                           sub_mesh;
    std::shared_ptr<asset::Material>                               material;
    core::Buffer                                                   material_data;
    rg::GPUBufferEdgeHandle                                        material_access;
    std::pmr::vector<std::pair<std::uint8_t, rg::GPUBufferHandle>> vertex_buffers;
    rg::GPUBufferHandle                                            indices;
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

auto MakeFrameConstant(const render::RenderRequest& request) -> PbrFrameConstant {
    const auto light = request.frame.lights.empty()
                           ? render::RenderLight{.position = {3.0f, -4.0f, 4.0f}, .color = math::Color::White(), .intensity = 30.0f}
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

auto CreateFrameTarget(render::RenderContext& context, const render::RenderRequest& request) -> rg::TextureHandle {
    if (context.graph.IsValid(request.target)) return request.target;

    const auto width  = context.swap_chain ? context.swap_chain->GetWidth() : 1u;
    const auto height = context.swap_chain ? context.swap_chain->GetHeight() : 1u;
    return context.graph.Create(gfx::TextureDesc{
        .name        = "PbrColor",
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color{0.02f, 0.025f, 0.03f, 1.0f},
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc,
    });
}

auto CreateDepthTarget(render::RenderContext& context, const gfx::TextureDesc& color_desc) -> rg::TextureHandle {
    return context.graph.Create(gfx::TextureDesc{
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

PbrRenderer::PbrRenderer(const asset::ResourceLoadContext& load_context)
    : IRenderer("PbrRenderer"),
      m_Device(load_context.device),
      m_LoadContext(load_context),
      m_Sampler(hitagi::gfx::Sampler::Create(m_Device, m_LoadContext.bindings, {
                                                                                   .name          = "PbrSampler",
                                                                                   .address_u     = gfx::AddressMode::Repeat,
                                                                                   .address_v     = gfx::AddressMode::Repeat,
                                                                                   .address_w     = gfx::AddressMode::Repeat,
                                                                                   .mag_filter    = gfx::FilterMode::Linear,
                                                                                   .min_filter    = gfx::FilterMode::Linear,
                                                                                   .mipmap_filter = gfx::FilterMode::Linear,
                                                                               })) {}

auto PbrRenderer::CountRenderableSubMeshes(const render::RenderRequest& request) const -> std::size_t {
    std::size_t count = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (IsRenderablePbrMaterial(sub_mesh.material)) ++count;
        }
    }
    return count;
}

auto PbrRenderer::Render(render::RenderContext& context, const render::RenderRequest& request) -> render::RenderResult {
    auto color = CreateFrameTarget(context, request);
    if (!context.graph.IsValid(color)) return {};

    const auto color_desc = context.graph.GetResourceDesc(color);
    auto       depth      = CreateDepthTarget(context, color_desc);
    const auto draw_count = CountRenderableSubMeshes(request);
    if (draw_count == 0) {
        return {.color = color, .depth = depth};
    }

    for (const auto& item : request.frame.draw_items) {
        if (item.mesh) item.mesh->Load(m_LoadContext);
        if (!item.mesh) continue;
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (sub_mesh.material) sub_mesh.material->Load(m_LoadContext);
        }
    }

    auto frame_constant    = context.graph.Create({
        .name   = "PbrFrameConstant",
        .size   = utils::align(sizeof(PbrFrameConstant), gfx::GPUBuffer::GetStorageViewRequirements(context.graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto instance_constant = context.graph.Create({
        .name   = "PbrInstanceConstant",
        .size   = sizeof(PbrInstanceConstant) * std::max<std::size_t>(1, request.frame.draw_items.size()),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto bindless_info     = context.graph.Create({
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
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (!IsRenderablePbrMaterial(sub_mesh.material)) continue;

            sub_mesh.material->Load(m_LoadContext);
            const auto* material_pass = sub_mesh.material->FindPass(kPbrForwardContract);
            if (!material_pass) continue;

            auto material_pipeline = CreatePbrMaterialPipeline(m_LoadContext, *material_pass);
            if (!material_pipeline) continue;

            auto material_data = material_pass->material_data;

            auto material_data_handle = context.graph.Create(
                {
                    .name   = std::pmr::string(std::format("PbrMaterialData-{}", draw_records.size())),
                    .size   = utils::align(material_data.GetDataSize(), gfx::GPUBuffer::GetStorageViewRequirements(context.graph.GetDevice()).size_alignment),
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
                if (const auto attr = item.mesh->vertices->GetAttributeData(mesh_attr); attr && attr->get().gpu_buffer) {
                    record.vertex_buffers.emplace_back(vertex_attr.binding, context.graph.Import(attr->get().gpu_buffer));
                }
            }
            record.indices = context.graph.Import(item.mesh->indices->GetGPUData());
        }
        ++instance_index;
    }

    auto                  sampler = context.graph.Import(m_Sampler);
    rg::RenderPassBuilder builder(context.graph);
    builder.SetName(std::format("PbrForwardPass-{}", context.graph.GetFrameIndex()));
    builder.SetRenderTarget(color, true);
    builder.SetDepthStencil(depth, true);
    const auto frame_constant_access    = builder.Read(frame_constant, {.offset = 0, .element_size = sizeof(PbrFrameConstant), .element_count = 1}, gfx::PipelineStage::VertexShader | gfx::PipelineStage::PixelShader);
    const auto instance_constant_access = builder.Read(instance_constant, {.offset = 0, .element_size = sizeof(PbrInstanceConstant), .element_count = std::max<std::size_t>(1, request.frame.draw_items.size())}, gfx::PipelineStage::VertexShader);
    const auto bindless_info_access     = builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(PbrBindlessInfo), .element_count = draw_count}, gfx::PipelineStage::All);
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
            cmd.SetIndexBuffer(pass.Resolve(record.indices), 0);
            cmd.DrawIndexed(record.sub_mesh.index_count, 1, record.sub_mesh.index_offset, record.sub_mesh.vertex_offset);
        }
    });
    builder.Finish();

    return {.color = color, .depth = depth};
}

}  // namespace hitagi::preset::pbr
