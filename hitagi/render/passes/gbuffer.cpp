module;

#include <range/v3/all.hpp>

module render;
import magic_enum;
import std;

namespace hitagi::render {

namespace {

auto ReadShaderSource(const std::filesystem::path& path) -> std::pmr::string {
    if (core::FileIOManager::Get() == nullptr) return {};
    return std::pmr::string(core::FileIOManager::Get()->SyncOpenAndReadBinary(path).Str());
}

}  // namespace
passes::GBuffer::GBuffer(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::GBuffer::EnsureResources() {
    if (m_AlbedoPipeline != nullptr &&
        m_NormalPipeline != nullptr &&
        m_MaterialPipeline != nullptr &&
        m_EmissivePipeline != nullptr) {
        return;
    }

    const auto source = ReadShaderSource(m_ShaderPath);

    m_VS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_AlbedoPS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-albedo-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSAlbedoMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_NormalPS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-normal-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSNormalMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_MaterialPS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-material-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMaterialMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_EmissivePS = m_Device.CreateShader({
        .name        = "deferred-gbuffer-emissive-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSEmissiveMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });

    const auto vertex_layout = m_Device.GetShaderCompiler().ExtractVertexLayout(m_VS->GetDesc());

    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& pixel_shader, gfx::Format format, bool depth_write) {
        return m_Device.CreateRenderPipeline({
            .name                = std::pmr::string(name),
            .shaders             = {m_VS, pixel_shader},
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
        });
    };

    m_AlbedoPipeline   = make_pipeline("deferred-gbuffer-albedo", m_AlbedoPS, gfx::Format::R32G32B32A32_FLOAT, true);
    m_NormalPipeline   = make_pipeline("deferred-gbuffer-normal", m_NormalPS, gfx::Format::R8G8B8A8_UNORM, false);
    m_MaterialPipeline = make_pipeline("deferred-gbuffer-material", m_MaterialPS, gfx::Format::R8G8B8A8_UNORM, false);
    m_EmissivePipeline = make_pipeline("deferred-gbuffer-emissive", m_EmissivePS, gfx::Format::R11G11B10_FLOAT, false);
}

auto passes::GBuffer::CreateTargets(RenderContext& context, const Desc& desc) -> GBufferOutput {
    auto& render_graph = context.graph;
    return GBufferOutput{
        .albedo = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_albedo_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.albedo_format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .normal = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_normal_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.normal_format,
            .clear_value = math::Color{0.5f, 0.5f, 1.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .material = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_material_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.material_format,
            .clear_value = math::Color{0.0f, 0.5f, 1.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .emissive = render_graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("deferred_gbuffer_emissive_{}", render_graph.GetFrameIndex())),
            .width       = desc.width,
            .height      = desc.height,
            .format      = desc.emissive_format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 0.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        }),
        .depth = render_graph.Create(gfx::TextureDesc{
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

auto passes::GBuffer::ImportAlbedoPipeline(RenderContext& context, std::string_view name) -> rg::RenderPipelineHandle {
    EnsureResources();
    return name.empty() ? context.graph.Import(m_AlbedoPipeline) : context.graph.Import(m_AlbedoPipeline, std::pmr::string(name));
}

auto passes::GBuffer::ImportNormalPipeline(RenderContext& context, std::string_view name) -> rg::RenderPipelineHandle {
    EnsureResources();
    return name.empty() ? context.graph.Import(m_NormalPipeline) : context.graph.Import(m_NormalPipeline, std::pmr::string(name));
}

auto passes::GBuffer::ImportMaterialPipeline(RenderContext& context, std::string_view name) -> rg::RenderPipelineHandle {
    EnsureResources();
    return name.empty() ? context.graph.Import(m_MaterialPipeline) : context.graph.Import(m_MaterialPipeline, std::pmr::string(name));
}

auto passes::GBuffer::ImportEmissivePipeline(RenderContext& context, std::string_view name) -> rg::RenderPipelineHandle {
    EnsureResources();
    return name.empty() ? context.graph.Import(m_EmissivePipeline) : context.graph.Import(m_EmissivePipeline, std::pmr::string(name));
}

void passes::GBuffer::BuildAlbedoPass(RenderContext& context, SceneDrawState& draw_state, const AlbedoPassDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.target) ||
        !render_graph.IsValid(desc.depth) ||
        !render_graph.IsValid(desc.frame_constant_buffer) ||
        !render_graph.IsValid(desc.instance_constant_buffer) ||
        !render_graph.IsValid(desc.bindless_info_buffer) ||
        !render_graph.IsValid(desc.sampler)) {
        return;
    }

    rg::RenderPassBuilder builder(render_graph);
    builder
        .SetName(desc.pass_name)
        .SetRenderTarget(desc.target, true)
        .SetDepthStencil(desc.depth, desc.clear_depth)
        .Read(desc.frame_constant_buffer, gfx::PipelineStage::VertexShader)
        .Read(desc.instance_constant_buffer, gfx::PipelineStage::VertexShader)
        .Read(desc.bindless_info_buffer)
        .AddSampler(desc.sampler);

    for (const auto& [_, material_info] : draw_state.material_infos) {
        builder.AddPipeline(material_info.pipeline);
        builder.Read(material_info.material_constant);
    }
    for (const auto& [_, material_instance_info] : draw_state.material_instance_infos) {
        for (const auto texture : material_instance_info.textures) {
            if (texture) {
                builder.Read(texture, {}, gfx::PipelineStage::PixelShader);
            }
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

    builder.SetExecutor([&draw_state, desc](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        gfx::GPUBufferView<SceneFrameConstant>(pass.Resolve(desc.frame_constant_buffer)).front() = desc.frame_constant;
        gfx::GPUBufferView<DrawBindlessInfo> bindless_infos(pass.Resolve(desc.bindless_info_buffer));

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

        for (auto* const material_instance : draw_state.active_material_instances) {
            auto       material_info            = draw_state.material_infos.at(material_instance->GetMaterial().get());
            auto&      material_constant_buffer = pass.Resolve(material_info.material_constant);
            const auto material_instance_index  = draw_state.material_instance_indices.at(material_instance);
            auto       material_constant_data   = material_instance->GenerateMaterialBuffer(desc.device_type == gfx::Device::Type::DX12);

            std::memcpy(
                material_constant_buffer.Map() + material_constant_buffer.AlignedElementSize() * material_instance_index,
                material_constant_data.GetData(),
                material_constant_data.GetDataSize());
            material_constant_buffer.UnMap();
        }

        auto instance_constant = gfx::GPUBufferView<SceneInstanceConstant>(pass.Resolve(desc.instance_constant_buffer));
        for (const auto& instance_info : draw_state.instance_infos) {
            instance_constant[instance_info.instance_index] = instance_info.instance_data;
        }

        std::size_t draw_index = 0;
        for (const auto& instance_info : draw_state.instance_infos) {
            const auto mesh_info = draw_state.mesh_infos.at(instance_info.mesh.get());

            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                const auto& material_instance_info = draw_state.material_instance_infos.at(sub_mesh.material_instance.get());
                const auto& material_info          = draw_state.material_infos.at(sub_mesh.material_instance->GetMaterial().get());
                auto&       pipeline               = pass.Resolve(material_info.pipeline);
                const auto  material_instance_index = draw_state.material_instance_indices.at(sub_mesh.material_instance.get());

                cmd.SetPipeline(pipeline);

                bindless_infos[draw_index] = {
                    .frame_constant    = pass.GetBindless(desc.frame_constant_buffer),
                    .instance_constant = pass.GetBindless(desc.instance_constant_buffer, instance_info.instance_index),
                    .material_constant = pass.GetBindless(material_info.material_constant, material_instance_index),
                    .sampler           = pass.GetBindless(desc.sampler),
                };

                for (auto [texture_bindless, texture_handle] : ranges::views::zip(bindless_infos[draw_index].textures, material_instance_info.textures)) {
                    if (texture_handle) {
                        texture_bindless = pass.GetBindless(texture_handle);
                    }
                }

                cmd.PushBindlessMetaInfo({
                    .handle = pass.GetBindless(desc.bindless_info_buffer, draw_index),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                    auto attr_handle = mesh_info.vertices[mesh_attr];
                    if (attr_handle) {
                        cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                    }
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0);
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
                ++draw_index;
            }
        }
    });

    builder.Finish();
}

void passes::GBuffer::BuildAttributePass(RenderContext& context, SceneDrawState& draw_state, const AttributePassDesc& desc) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(desc.target) ||
        !render_graph.IsValid(desc.depth) ||
        !render_graph.IsValid(desc.dependency) ||
        !render_graph.IsValid(desc.frame_constant) ||
        !render_graph.IsValid(desc.instance_constant) ||
        !render_graph.IsValid(desc.bindless_info) ||
        !render_graph.IsValid(desc.sampler) ||
        !render_graph.IsValid(desc.pipeline)) {
        return;
    }

    rg::RenderPassBuilder builder(render_graph);
    builder
        .SetName(desc.pass_name)
        .SetRenderTarget(desc.target, true)
        .ReadDepthStencil(desc.depth)
        .Read(desc.dependency, {}, gfx::PipelineStage::PixelShader)
        .Read(desc.frame_constant, gfx::PipelineStage::VertexShader)
        .Read(desc.instance_constant, gfx::PipelineStage::VertexShader)
        .Read(desc.bindless_info)
        .AddSampler(desc.sampler)
        .AddPipeline(desc.pipeline);

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
        builder.Read(material_info.material_constant);
    }
    for (const auto& [_, material_instance_info] : draw_state.material_instance_infos) {
        for (const auto texture : material_instance_info.textures) {
            if (texture) {
                builder.Read(texture, {}, gfx::PipelineStage::PixelShader);
            }
        }
    }

    builder.SetExecutor([&draw_state, desc](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        gfx::GPUBufferView<DrawBindlessInfo> bindless_infos(pass.Resolve(desc.bindless_info));

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

        auto& pipeline = pass.Resolve(desc.pipeline);
        cmd.SetPipeline(pipeline);

        std::size_t draw_index = 0;
        for (const auto& instance_info : draw_state.instance_infos) {
            const auto mesh_info = draw_state.mesh_infos.at(instance_info.mesh.get());

            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                const auto& material_instance_info  = draw_state.material_instance_infos.at(sub_mesh.material_instance.get());
                const auto& material_info           = draw_state.material_infos.at(sub_mesh.material_instance->GetMaterial().get());
                const auto  material_instance_index = draw_state.material_instance_indices.at(sub_mesh.material_instance.get());

                bindless_infos[draw_index] = {
                    .frame_constant    = pass.GetBindless(desc.frame_constant),
                    .instance_constant = pass.GetBindless(desc.instance_constant, instance_info.instance_index),
                    .material_constant = pass.GetBindless(material_info.material_constant, material_instance_index),
                    .sampler           = pass.GetBindless(desc.sampler),
                };

                for (auto [texture_bindless, texture_handle] : ranges::views::zip(bindless_infos[draw_index].textures, material_instance_info.textures)) {
                    if (texture_handle) {
                        texture_bindless = pass.GetBindless(texture_handle);
                    }
                }

                cmd.PushBindlessMetaInfo({
                    .handle = pass.GetBindless(desc.bindless_info, draw_index),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                    auto attr_handle = mesh_info.vertices[mesh_attr];
                    if (attr_handle) {
                        cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                    }
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0);
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
                ++draw_index;
            }
        }
    });

    builder.Finish();
}

}  // namespace hitagi::render
