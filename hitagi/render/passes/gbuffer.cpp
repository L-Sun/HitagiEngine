module;

#include <range/v3/all.hpp>

module render;
import magic_enum;
import std;

namespace hitagi::render {

namespace {

auto CountDraws(const RenderDrawState& draw_state) noexcept -> std::size_t {
    std::size_t result = 0;
    for (const auto& instance_info : draw_state.instance_infos) {
        result += instance_info.mesh->sub_meshes.size();
    }
    return std::max<std::size_t>(1, result);
}

void UploadMaterialData(
    const asset::MaterialPass& material_pass,
    gfx::GPUBuffer&            material_data_buffer) {
    const auto& material_data = material_pass.material_data;
    if (material_data.Empty()) return;

    auto* const mapped_data = static_cast<std::byte*>(material_data_buffer.Map());
    std::memcpy(mapped_data, material_data.GetData(), material_data.GetDataSize());
    material_data_buffer.UnMap();
}

}  // namespace
auto passes::DepthPrepass::CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle {
    return context.graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("depth_prepass_{}", context.graph.GetFrameIndex())),
        .width       = desc.width,
        .height      = desc.height,
        .format      = desc.format,
        .clear_value = gfx::ClearDepthStencil{
            .depth   = 1.0f,
            .stencil = 0,
        },
        .usages = gfx::TextureUsageFlags::DepthStencil | gfx::TextureUsageFlags::SRV,
    });
}

void passes::DepthPrepass::Build(RenderContext& context, const BuildDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.depth) ||
        !render_graph.IsValid(desc.frame_constant) ||
        !render_graph.IsValid(desc.instance_constant) ||
        !desc.pipeline) {
        return;
    }

    const auto pass_name      = desc.pass_name.empty()
                                    ? std::pmr::string(std::format("DepthPrepass-{}", render_graph.GetFrameIndex()))
                                    : desc.pass_name;
    const auto scratch_target = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("{}_scratch_color", pass_name)),
        .width       = desc.width,
        .height      = desc.height,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget,
    });

    rg::RenderPassBuilder pass_builder(render_graph);
    pass_builder.SetName(pass_name);
    pass_builder.SetRenderTarget(scratch_target, true);
    pass_builder.SetDepthStencil(desc.depth, desc.clear_depth);
    pass_builder.Read(desc.frame_constant, {.element_count = 0}, gfx::PipelineStage::VertexShader);
    pass_builder.Read(desc.instance_constant, {.element_count = 0}, gfx::PipelineStage::VertexShader);
    pass_builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(desc.width),
            .height = static_cast<float>(desc.height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = desc.width,
            .height = desc.height,
        });
        cmd.SetPipeline(*desc.pipeline);
    });
    pass_builder.Finish();
}

auto passes::ShadowMapPass::CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle {
    return context.graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("shadow_map_{}", context.graph.GetFrameIndex())),
        .width       = desc.width,
        .height      = desc.height,
        .format      = desc.format,
        .clear_value = gfx::ClearDepthStencil{
            .depth   = 1.0f,
            .stencil = 0,
        },
        .usages = gfx::TextureUsageFlags::DepthStencil | gfx::TextureUsageFlags::SRV,
    });
}

void passes::ShadowMapPass::Build(RenderContext& context, const BuildDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.shadow_map) ||
        !render_graph.IsValid(desc.light_frame_constant) ||
        !render_graph.IsValid(desc.instance_constant) ||
        !desc.pipeline) {
        return;
    }

    const auto pass_name      = desc.pass_name.empty()
                                    ? std::pmr::string(std::format("ShadowMapPass-{}", render_graph.GetFrameIndex()))
                                    : desc.pass_name;
    const auto scratch_target = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("{}_scratch_color", pass_name)),
        .width       = desc.width,
        .height      = desc.height,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget,
    });

    rg::RenderPassBuilder pass_builder(render_graph);
    pass_builder.SetName(pass_name);
    pass_builder.SetRenderTarget(scratch_target, true);
    pass_builder.SetDepthStencil(desc.shadow_map, desc.clear_depth);
    pass_builder.Read(desc.light_frame_constant, {.element_count = 0}, gfx::PipelineStage::VertexShader);
    pass_builder.Read(desc.instance_constant, {.element_count = 0}, gfx::PipelineStage::VertexShader);
    pass_builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(desc.width),
            .height = static_cast<float>(desc.height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = desc.width,
            .height = desc.height,
        });
        cmd.SetPipeline(*desc.pipeline);
    });
    pass_builder.Finish();
}

auto passes::ObjectMaterialIdPass::CreateTarget(RenderContext& context, const Desc& desc) -> rg::TextureHandle {
    return context.graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("object_material_id_{}", context.graph.GetFrameIndex())),
        .width       = desc.width,
        .height      = desc.height,
        .format      = desc.format,
        .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
}

passes::GBuffer::GBuffer(gfx::Device& device, ShaderSource shader)
    : m_Device(device),
      m_Shader(std::move(shader)) {}

void passes::GBuffer::EnsureResources() {
    if (m_AlbedoPipeline != nullptr &&
        m_NormalPipeline != nullptr &&
        m_MaterialPipeline != nullptr &&
        m_EmissivePipeline != nullptr) {
        return;
    }

    m_VS         = m_Device.CreateShader({
        .name        = "deferred-gbuffer-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_AlbedoPS   = m_Device.CreateShader({
        .name        = "deferred-gbuffer-albedo-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSAlbedoMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_NormalPS   = m_Device.CreateShader({
        .name        = "deferred-gbuffer-normal-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSNormalMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_MaterialPS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-material-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMaterialMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });
    m_EmissivePS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-emissive-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSEmissiveMain",
        .source_code = m_Shader.code,
        .path        = m_Shader.path,
    });

    const auto vertex_layout = m_Device.GetShaderCompiler().ExtractVertexLayout(m_VS->GetDesc());

    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& pixel_shader, gfx::Format format, bool depth_write) {
        return m_Device.CreateRenderPipeline(
            {
                .name                = std::pmr::string(name),
                .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
                .vertex_input_layout = vertex_layout,
                .rasterization_state = {.cull_mode = gfx::CullMode::None},
                .depth_stencil_state = {
                    .depth_test_enable  = true,
                    .depth_write_enable = depth_write,
                    .depth_compare_op   = depth_write ? gfx::CompareOp::Less : gfx::CompareOp::LessEqual,
                },
                .render_format        = format,
                .depth_stencil_format = gfx::Format::D32_FLOAT,
            },
            {m_VS, pixel_shader});
    };

    m_AlbedoPipeline   = make_pipeline("deferred-gbuffer-albedo", m_AlbedoPS, gfx::Format::R32G32B32A32_FLOAT, true);
    m_NormalPipeline   = make_pipeline("deferred-gbuffer-normal", m_NormalPS, gfx::Format::R8G8B8A8_UNORM, false);
    m_MaterialPipeline = make_pipeline("deferred-gbuffer-material", m_MaterialPS, gfx::Format::R8G8B8A8_UNORM, false);
    m_EmissivePipeline = make_pipeline("deferred-gbuffer-emissive", m_EmissivePS, gfx::Format::R11G11B10_FLOAT, false);
}

auto passes::GBuffer::CreateTargets(RenderContext& context, const Desc& desc) -> GBufferOutput {
    auto& render_graph = context.graph;
    return GBufferOutput{
        .albedo             = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_albedo_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.albedo_format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .normal             = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_normal_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.normal_format,
            .clear_value = math::Color{0.5f, 0.5f, 1.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .material           = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_material_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.material_format,
            .clear_value = math::Color{0.0f, 0.5f, 1.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .emissive           = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_emissive_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.emissive_format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .object_material_id = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_object_material_id_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.object_material_id_format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .depth              = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_depth_stencil_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = gfx::Format::D32_FLOAT,
            .clear_value = gfx::ClearDepthStencil{
                .depth   = 1.0f,
                .stencil = 0,
            },
            .usages = gfx::TextureUsageFlags::DepthStencil,
        }),
    };
}

auto passes::GBuffer::GetAlbedoPipeline() -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources();
    return m_AlbedoPipeline;
}

auto passes::GBuffer::GetNormalPipeline() -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources();
    return m_NormalPipeline;
}

auto passes::GBuffer::GetMaterialPipeline() -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources();
    return m_MaterialPipeline;
}

auto passes::GBuffer::GetEmissivePipeline() -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources();
    return m_EmissivePipeline;
}

void passes::GBuffer::BuildAlbedoPass(RenderContext& context, RenderDrawState& draw_state, const AlbedoPassDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.target) ||
        !render_graph.IsValid(desc.depth) ||
        !render_graph.IsValid(desc.frame_constant_buffer) ||
        !render_graph.IsValid(desc.instance_constant_buffer) ||
        !render_graph.IsValid(desc.bindless_info_buffer) ||
        !render_graph.IsValid(desc.sampler)) {
        return;
    }

    const auto draw_count = CountDraws(draw_state);

    std::unordered_map<rg::GPUBufferHandle, rg::GPUBufferEdgeHandle> material_accesses;
    rg::RenderPassBuilder                                            builder(render_graph);
    builder.SetName(desc.pass_name);
    builder.SetRenderTarget(desc.target, true);
    builder.SetDepthStencil(desc.depth, desc.clear_depth);
    const auto frame_constant_buffer_access    = builder.Read(desc.frame_constant_buffer, {.offset = 0, .element_size = sizeof(FrameConstant), .element_count = 1}, gfx::PipelineStage::VertexShader);
    const auto instance_constant_buffer_access = builder.Read(desc.instance_constant_buffer, {.offset = 0, .element_size = sizeof(InstanceConstant), .element_count = std::max<std::size_t>(1, draw_state.instance_infos.size()), .element_stride = desc.instance_stride}, gfx::PipelineStage::VertexShader);
    const auto bindless_info_buffer_access     = builder.Read(desc.bindless_info_buffer, {.offset = 0, .element_size = sizeof(DrawBindlessInfo), .element_count = std::max<std::size_t>(1, draw_count), .element_stride = desc.bindless_stride}, gfx::PipelineStage::All);
    builder.AddSampler(desc.sampler);

    for (const auto& [_, material_info] : draw_state.material_infos) {
        if (!material_info.pass_participation.Participates(MaterialPass::GBuffer)) continue;
        const auto* material_pass = material_info.material ? material_info.material->FindPass(material_info.material_pass_contract) : nullptr;
        if (material_pass) {
            material_accesses[material_info.material_data] = builder.Read(material_info.material_data, {.offset = 0, .element_size = material_pass->material_data.GetDataSize(), .element_count = 1});
        }
    }
    for (const auto& [_, mesh_info] : draw_state.mesh_infos) {
        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            const auto handle = mesh_info.vertices[attr];
            if (handle) {
                builder.ReadAsVertices(handle);
            }
        });
        builder.ReadAsIndices(mesh_info.indices);
    }

    builder.SetExecutor([&draw_state, desc, material_accesses, frame_constant_buffer_access, instance_constant_buffer_access, bindless_info_buffer_access](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        pass.Resolve(frame_constant_buffer_access).GetMappedSpan<FrameConstant>().front() = desc.frame_constant;
        auto bindless_infos                                                                   = pass.Resolve(bindless_info_buffer_access).GetMappedSpan<DrawBindlessInfo>();

        const auto& render_target = pass.Resolve(desc.target);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });

        for (auto* const material : draw_state.active_materials) {
            auto  material_info        = draw_state.material_infos.at(material);
            auto& material_data_buffer = pass.Resolve(material_info.material_data);
            if (!material_info.pass_participation.Participates(MaterialPass::GBuffer)) continue;
            const auto* material_pass = material_info.material ? material_info.material->FindPass(material_info.material_pass_contract) : nullptr;
            if (!material_pass) continue;
            UploadMaterialData(
                *material_pass,
                material_data_buffer);
        }

        auto instance_constant = pass.Resolve(instance_constant_buffer_access).GetMappedSpan<InstanceConstant>();
        for (const auto& instance_info : draw_state.instance_infos) {
            instance_constant[instance_info.instance_index] = instance_info.instance_data;
        }

        std::size_t draw_index = 0;
        for (const auto& instance_info : draw_state.instance_infos) {
            const auto mesh_info = draw_state.mesh_infos.at(instance_info.mesh.get());

            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                if (!sub_mesh.material) continue;
                const auto material_info_iter = draw_state.material_infos.find(sub_mesh.material.get());
                if (material_info_iter == draw_state.material_infos.end()) continue;
                const auto& material_info = material_info_iter->second;
                if (!material_info.pass_participation.Participates(MaterialPass::GBuffer)) continue;
                if (!material_info.pipeline) continue;
                auto& pipeline = *material_info.pipeline;

                cmd.SetPipeline(pipeline);

                bindless_infos[draw_index] = {
                    .frame_constant    = pass.Resolve(frame_constant_buffer_access).GetBindlessHandle(),
                    .instance_constant = pass.Resolve(instance_constant_buffer_access).GetBindlessHandle(),
                    .instance_index    = static_cast<std::uint32_t>(instance_info.instance_index),
                    .instance_stride   = static_cast<std::uint32_t>(pass.Resolve(instance_constant_buffer_access).GetDesc().element_stride),
                    .material_data     = pass.Resolve(material_accesses.at(material_info.material_data)).GetBindlessHandle(),
                    .sampler           = pass.Resolve(desc.sampler).GetBindlessHandle(),
                };

                cmd.PushBindlessMetaInfo({
                    .handle        = pass.Resolve(bindless_info_buffer_access).GetBindlessHandle(),
                    .record_index  = static_cast<std::uint32_t>(draw_index),
                    .record_stride = static_cast<std::uint32_t>(pass.Resolve(bindless_info_buffer_access).GetDesc().element_stride),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                    auto attr_handle = mesh_info.vertices[mesh_attr];
                    if (attr_handle) {
                        cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                    }
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0, mesh_info.index_format);
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
                ++draw_index;
            }
        }
    });

    builder.Finish();
}

void passes::GBuffer::BuildAttributePass(RenderContext& context, RenderDrawState& draw_state, const AttributePassDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.target) ||
        !render_graph.IsValid(desc.depth) ||
        !render_graph.IsValid(desc.dependency) ||
        !render_graph.IsValid(desc.frame_constant) ||
        !render_graph.IsValid(desc.instance_constant) ||
        !render_graph.IsValid(desc.bindless_info) ||
        !render_graph.IsValid(desc.sampler) ||
        !desc.pipeline) {
        return;
    }

    const auto draw_count = CountDraws(draw_state);

    std::unordered_map<rg::GPUBufferHandle, rg::GPUBufferEdgeHandle> material_accesses;
    rg::RenderPassBuilder                                            builder(render_graph);
    builder.SetName(desc.pass_name);
    builder.SetRenderTarget(desc.target, true);
    builder.ReadDepthStencil(desc.depth);
    builder.Read(desc.dependency, {}, gfx::PipelineStage::PixelShader);
    const auto frame_constant_access    = builder.Read(desc.frame_constant, {.offset = 0, .element_size = sizeof(FrameConstant), .element_count = 1}, gfx::PipelineStage::VertexShader);
    const auto instance_constant_access = builder.Read(desc.instance_constant, {.offset = 0, .element_size = sizeof(InstanceConstant), .element_count = std::max<std::size_t>(1, draw_state.instance_infos.size()), .element_stride = desc.instance_stride}, gfx::PipelineStage::VertexShader);
    const auto bindless_info_access     = builder.Read(desc.bindless_info, {.offset = 0, .element_size = sizeof(DrawBindlessInfo), .element_count = std::max<std::size_t>(1, draw_count), .element_stride = desc.bindless_stride}, gfx::PipelineStage::All);
    builder.AddSampler(desc.sampler);

    for (const auto& [mesh, mesh_info] : draw_state.mesh_infos) {
        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            const auto handle = mesh_info.vertices[attr];
            if (handle) {
                builder.ReadAsVertices(handle);
            }
        });
        builder.ReadAsIndices(mesh_info.indices);
    }
    for (const auto& [_, material_info] : draw_state.material_infos) {
        if (!material_info.pass_participation.Participates(MaterialPass::GBuffer)) continue;
        const auto* material_pass = material_info.material ? material_info.material->FindPass(material_info.material_pass_contract) : nullptr;
        if (material_pass) {
            material_accesses[material_info.material_data] = builder.Read(material_info.material_data, {.offset = 0, .element_size = material_pass->material_data.GetDataSize(), .element_count = 1});
        }
    }
    builder.SetExecutor([&draw_state, desc, material_accesses, frame_constant_access, instance_constant_access, bindless_info_access](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        auto bindless_infos = pass.Resolve(bindless_info_access).GetMappedSpan<DrawBindlessInfo>();

        const auto& render_target = pass.Resolve(desc.target);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });

        auto& pipeline = *desc.pipeline;
        cmd.SetPipeline(pipeline);

        std::size_t draw_index = 0;
        for (const auto& instance_info : draw_state.instance_infos) {
            const auto mesh_info = draw_state.mesh_infos.at(instance_info.mesh.get());

            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                if (!sub_mesh.material) continue;
                const auto material_info_iter = draw_state.material_infos.find(sub_mesh.material.get());
                if (material_info_iter == draw_state.material_infos.end()) continue;
                const auto& material_info = material_info_iter->second;
                if (!material_info.pass_participation.Participates(MaterialPass::GBuffer)) continue;
                bindless_infos[draw_index] = {
                    .frame_constant    = pass.Resolve(frame_constant_access).GetBindlessHandle(),
                    .instance_constant = pass.Resolve(instance_constant_access).GetBindlessHandle(),
                    .instance_index    = static_cast<std::uint32_t>(instance_info.instance_index),
                    .instance_stride   = static_cast<std::uint32_t>(pass.Resolve(instance_constant_access).GetDesc().element_stride),
                    .material_data     = pass.Resolve(material_accesses.at(material_info.material_data)).GetBindlessHandle(),
                    .sampler           = pass.Resolve(desc.sampler).GetBindlessHandle(),
                };

                cmd.PushBindlessMetaInfo({
                    .handle        = pass.Resolve(bindless_info_access).GetBindlessHandle(),
                    .record_index  = static_cast<std::uint32_t>(draw_index),
                    .record_stride = static_cast<std::uint32_t>(pass.Resolve(bindless_info_access).GetDesc().element_stride),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                    auto attr_handle = mesh_info.vertices[mesh_attr];
                    if (attr_handle) {
                        cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                    }
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0, mesh_info.index_format);
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
                ++draw_index;
            }
        }
    });

    builder.Finish();
}

}  // namespace hitagi::render
