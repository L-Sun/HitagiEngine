module;

#include <imgui.h>
#include <range/v3/all.hpp>
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#include <cstdlib>

#undef near
#undef far

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

auto DeferredRenderer::Render(RenderContext& context, const SceneView& view, rg::TextureHandle target) -> rg::TextureHandle {
    if (view.scene == nullptr || view.camera == nullptr) return target;
    return RenderScene(context, view, target);
}

passes::EditorGrid::EditorGrid(gfx::Device& device, std::filesystem::path shader_path) {
    const auto shader_source = core::FileIOManager::Get()
                                   ? std::pmr::string(core::FileIOManager::Get()->SyncOpenAndReadBinary(shader_path).Str())
                                   : std::pmr::string{};

    m_VS = device.CreateShader({
        .name        = "viewport-grid-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = shader_source,
        .path        = shader_path,
    });
    m_PS = device.CreateShader({
        .name        = "viewport-grid-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = shader_source,
        .path        = shader_path,
    });
    m_Pipeline = device.CreateRenderPipeline({
        .name           = "viewport-grid",
        .shaders        = {m_VS, m_PS},
        .assembly_state = {
            .primitive = gfx::PrimitiveTopology::TriangleList,
        },
        .rasterization_state = {
            .cull_mode               = gfx::CullMode::None,
            .front_counter_clockwise = false,
        },
        .blend_state = {
            .blend_enable           = true,
            .src_color_blend_factor = gfx::BlendFactor::SrcAlpha,
            .dst_color_blend_factor = gfx::BlendFactor::InvSrcAlpha,
            .color_blend_op         = gfx::BlendOp::Add,
            .src_alpha_blend_factor = gfx::BlendFactor::One,
            .dst_alpha_blend_factor = gfx::BlendFactor::InvSrcAlpha,
            .alpha_blend_op         = gfx::BlendOp::Add,
        },
        .render_format = gfx::Format::R8G8B8A8_UNORM,
    });
}

auto passes::EditorGrid::Build(RenderContext& context, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) || !m_Pipeline) return target;
    const auto output = render_graph.MoveFrom(target, std::format("Viewport Grid Target {}", render_graph.GetFrameIndex()));

    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;
    const auto        view            = math::look_at(global_eye, global_look_dir, global_up);
    const auto        projection      = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto        forward         = math::normalize(global_look_dir);
    const auto        forward_z       = std::abs(forward.z);
    const auto        below_dist      = std::abs(global_eye.z);
    const auto        look_dist       = forward_z > 1e-4f ? below_dist / forward_z : below_dist * 10.0f;
    const auto        grid_distance   = std::max(std::lerp(look_dist, below_dist, 1.0f - forward_z), 0.1f);
    const auto        base_step       = std::clamp(std::pow(10.0f, std::floor(std::log10(grid_distance))), 0.1f, 100.0f);
    const auto        next_step       = std::clamp(base_step * 10.0f, 0.1f, 1000.0f);
    const auto        fade            = std::clamp((grid_distance - base_step) / std::max(next_step - base_step, 1e-5f), 0.0f, 1.0f);

    const auto target_desc          = render_graph.GetResourceDesc(output);
    const auto grid_constant_handle = render_graph.Create(
        {
            .name         = "viewport_grid_constant",
            .element_size = sizeof(ViewportGridConstant),
            .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "viewport_grid_constant");
    const auto bindless_info_handle = render_graph.Create(
        {
            .name         = "viewport_grid_bindless",
            .element_size = sizeof(ViewportGridBindlessInfo),
            .usages       = gfx::GPUBufferUsageFlags::Constant | gfx::GPUBufferUsageFlags::MapWrite,
        },
        "viewport_grid_bindless");
    const auto pipeline_handle = render_graph.Import(m_Pipeline, "viewport_grid_pipeline");

    rg::RenderPassBuilder builder(render_graph);
    builder.SetName(std::format("ViewportGridPass-{}", render_graph.GetFrameIndex()))
        .Read(grid_constant_handle)
        .Read(bindless_info_handle)
        .AddPipeline(pipeline_handle)
        .SetRenderTarget(output, false)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();

            gfx::GPUBufferView<ViewportGridConstant>(pass.Resolve(grid_constant_handle)).front() = ViewportGridConstant{
                .inv_proj_view                = math::inverse(projection * view),
                .camera_pos                   = {global_eye, 1.0f},
                .camera_forward               = {forward, 0.0f},
                .viewport_size_base_step_fade = {static_cast<float>(target_desc.width), static_cast<float>(target_desc.height), base_step, fade},
                .clip_and_opacity             = {camera.parameters.far_clip, 0.65f, 0.0f, 0.0f},
            };

            auto bindless_infos                   = gfx::GPUBufferView<ViewportGridBindlessInfo>(pass.Resolve(bindless_info_handle));
            bindless_infos.front().grid_constant = pass.GetBindless(grid_constant_handle);

            const auto& render_target = pass.Resolve(output);
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
            cmd.SetPipeline(pass.Resolve(pipeline_handle));
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info_handle),
            });
            cmd.Draw(3);
        })
        .Finish();
    return output;
}

void passes::Present::Build(RenderContext& context, rg::TextureHandle input) {
    if (context.swap_chain == nullptr || !context.graph.IsValid(input)) return;

    rg::PresentPassBuilder(context.graph)
        .From(input)
        .SetSwapChain(context.swap_chain)
        .Finish();
}

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

passes::EditorSelectionMetadata::EditorSelectionMetadata(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::EditorSelectionMetadata::EnsureResources() {
    if (m_IdPipeline && m_VisualPipeline && m_DepthPipeline) return;

    const auto source = ReadShaderSource(m_ShaderPath);

    m_VS = m_Device.CreateShader({
        .name        = "editor-selection-mask-vs",
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSSelectionMaskMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_IdPS = m_Device.CreateShader({
        .name        = "editor-selection-id-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionIdMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_VisualPS = m_Device.CreateShader({
        .name        = "editor-selection-visual-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionVisualMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });
    m_DepthPS = m_Device.CreateShader({
        .name        = "editor-selection-depth-ps",
        .type        = gfx::ShaderType::Pixel,
        .entry       = "PSSelectionDepthMain",
        .source_code = source,
        .path        = m_ShaderPath,
    });

    const auto vertex_layout = m_Device.GetShaderCompiler().ExtractVertexLayout(m_VS->GetDesc());

    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& ps, gfx::Format format, bool depth_write) {
        return m_Device.CreateRenderPipeline({
            .name                = std::pmr::string(name),
            .shaders             = {m_VS, ps},
            .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
            .vertex_input_layout = vertex_layout,
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .depth_stencil_state = {
                .depth_test_enable  = true,
                .depth_write_enable = depth_write,
                .depth_compare_op   = gfx::CompareOp::LessEqual,
            },
            .render_format        = format,
            .depth_stencil_format = gfx::Format::D32_FLOAT,
        });
    };

    m_IdPipeline     = make_pipeline("editor-selection-id", m_IdPS, gfx::Format::R32_UINT, true);
    m_VisualPipeline = make_pipeline("editor-selection-visual", m_VisualPS, gfx::Format::R32_UINT, false);
    m_DepthPipeline  = make_pipeline("editor-selection-depth", m_DepthPS, gfx::Format::R32_FLOAT, false);
}

auto passes::EditorSelectionMetadata::ImportPipeline(RenderContext& context, Target target) -> rg::RenderPipelineHandle {
    EnsureResources();
    switch (target) {
        case Target::Id:
            return context.graph.Import(m_IdPipeline);
        case Target::Visual:
            return context.graph.Import(m_VisualPipeline);
        case Target::Depth:
            return context.graph.Import(m_DepthPipeline);
    }
    return {};
}

auto passes::EditorSelectionMetadata::Build(
    RenderContext&             context,
    SceneDrawState&            draw_state,
    rg::GPUBufferHandle        frame_constant,
    const EditorSelectionDesc& desc,
    std::uint32_t              width,
    std::uint32_t              height) -> EditorSelectionBuffers {
    auto& render_graph = context.graph;
    if (!desc.enabled || desc.items.empty() || draw_state.instance_infos.empty() || !render_graph.IsValid(frame_constant)) return {};

    std::pmr::vector<EditorSelectionItem> selected_items;
    selected_items.reserve(desc.items.size());
    for (const auto& item : desc.items) {
        if (item.entity && item.selection_id != 0) selected_items.emplace_back(item);
    }
    if (selected_items.empty()) return {};

    std::pmr::vector<std::size_t> selected_instances;
    for (std::size_t i = 0; i < draw_state.instance_infos.size(); ++i) {
        const auto entity = draw_state.instance_infos[i].entity;
        const auto item_it = ranges::find_if(selected_items, [entity](const EditorSelectionItem& item) {
            return item.entity == entity;
        });
        if (item_it != selected_items.end()) selected_instances.emplace_back(i);
    }
    if (selected_instances.empty()) return {};

    const auto id = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_id_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_UINT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto visual = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_visual_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_UINT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto selection_depth = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_depth_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R32_FLOAT,
        .clear_value = math::Color::Black(),
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
    });
    const auto depth_stencil = render_graph.Create(gfx::TextureDesc{
        .name        = std::pmr::string(std::format("editor_selection_depth_stencil_{}", render_graph.GetFrameIndex())),
        .width       = width,
        .height      = height,
        .format      = gfx::Format::D32_FLOAT,
        .clear_value = gfx::ClearDepthStencil{.depth = 1.0f, .stencil = 0},
        .usages      = gfx::TextureUsageFlags::DepthStencil,
    });

    auto instance_constant = render_graph.Create({
        .name          = "editor_selection_instance_constant",
        .element_size  = sizeof(InstanceConstant),
        .element_count = selected_instances.size(),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    auto bindless_info = render_graph.Create({
        .name          = "editor_selection_bindless_infos",
        .element_size  = sizeof(BindlessInfo),
        .element_count = selected_instances.size(),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    std::pmr::vector<InstanceConstant> selected_constants;
    selected_constants.resize(selected_instances.size());
    for (std::size_t selection_index = 0; selection_index < selected_instances.size(); ++selection_index) {
        const auto& instance = draw_state.instance_infos[selected_instances[selection_index]];
        const auto item_it = ranges::find_if(selected_items, [entity = instance.entity](const EditorSelectionItem& item) {
            return item.entity == entity;
        });
        const auto visual_id = item_it != selected_items.end()
                                   ? static_cast<std::uint32_t>(item_it->visual)
                                   : static_cast<std::uint32_t>(EditorSelectionVisual::Selected);
        selected_constants[selection_index] = {
            .model        = instance.instance_data.model,
            .selection_id = item_it != selected_items.end() ? item_it->selection_id : 1u,
            .visual_id    = visual_id,
        };
    }

    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, id, depth_stencil, ImportPipeline(context, Target::Id), Target::Id, selected_constants, selected_instances);
    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, visual, depth_stencil, ImportPipeline(context, Target::Visual), Target::Visual, selected_constants, selected_instances);
    BuildTargetPass(context, draw_state, frame_constant, instance_constant, bindless_info, selection_depth, depth_stencil, ImportPipeline(context, Target::Depth), Target::Depth, selected_constants, selected_instances);

    return {
        .id     = id,
        .visual = visual,
        .depth  = selection_depth,
    };
}

void passes::EditorSelectionMetadata::BuildTargetPass(
    RenderContext&             context,
    SceneDrawState&            draw_state,
    rg::GPUBufferHandle        frame_constant,
    rg::GPUBufferHandle        instance_constant,
    rg::GPUBufferHandle        bindless_info,
    rg::TextureHandle          target,
    rg::TextureHandle          depth_stencil,
    rg::RenderPipelineHandle   pipeline,
    Target                     target_kind,
    std::span<const InstanceConstant> selected_constants,
    std::span<const std::size_t> selected_instances) {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) || !render_graph.IsValid(depth_stencil) || !render_graph.IsValid(pipeline)) return;

    auto pass_name = std::format(
        "EditorSelection{}Pass-{}",
        target_kind == Target::Id ? "Id" : target_kind == Target::Visual ? "Visual" : "Depth",
        render_graph.GetFrameIndex());

    rg::RenderPassBuilder builder(render_graph);
    builder
        .SetName(pass_name)
        .SetRenderTarget(target, true)
        .Read(frame_constant, gfx::PipelineStage::VertexShader)
        .Read(instance_constant, gfx::PipelineStage::VertexShader)
        .Read(bindless_info)
        .AddPipeline(pipeline);

    if (target_kind == Target::Id) {
        builder.SetDepthStencil(depth_stencil, true);
    } else {
        builder.ReadDepthStencil(depth_stencil);
    }

    for (const auto& [_, mesh_info] : draw_state.mesh_infos) {
        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            const auto handle = mesh_info.vertices[attr];
            if (handle) builder.ReadAsVertices(handle);
        });
        builder.ReadAsIndices(mesh_info.indices);
    }

    std::pmr::vector<std::size_t> selected_instances_copy{selected_instances.begin(), selected_instances.end()};
    std::pmr::vector<InstanceConstant> selected_constants_copy{selected_constants.begin(), selected_constants.end()};
    builder.SetExecutor([&draw_state, frame_constant, instance_constant, bindless_info, target, pipeline, selected_instances_copy, selected_constants_copy](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        auto instance_constants = gfx::GPUBufferView<InstanceConstant>(pass.Resolve(instance_constant));
        auto bindless_infos = gfx::GPUBufferView<BindlessInfo>(pass.Resolve(bindless_info));
        for (std::size_t i = 0; i < selected_instances_copy.size(); ++i) {
            instance_constants[i] = selected_constants_copy[i];
            bindless_infos[i] = {
                .frame_constant    = pass.GetBindless(frame_constant),
                .instance_constant = pass.GetBindless(instance_constant, i),
            };
        }

        const auto& render_target = pass.Resolve(target);
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

        auto& pipeline_ref = pass.Resolve(pipeline);
        cmd.SetPipeline(pipeline_ref);

        for (std::size_t selection_index = 0; selection_index < selected_instances_copy.size(); ++selection_index) {
            const auto& instance_info = draw_state.instance_infos[selected_instances_copy[selection_index]];
            const auto  mesh_info     = draw_state.mesh_infos.at(instance_info.mesh.get());

            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info, selection_index),
            });
            for (const auto& vertex_attr : pipeline_ref.GetDesc().vertex_input_layout) {
                auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                auto attr_handle = mesh_info.vertices[mesh_attr];
                if (attr_handle) {
                    cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                }
            }
            cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0);
            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
            }
        }
    });
    builder.Finish();
}

passes::SelectionOutline::SelectionOutline(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::SelectionOutline::EnsureResources(gfx::Format target_format) {
    if (m_Pipeline && m_TargetFormat == target_format) return;

    const auto source = ReadShaderSource(m_ShaderPath);

    if (m_VS == nullptr) {
        m_VS = m_Device.CreateShader({
            .name        = "selection-outline-vs",
            .type        = gfx::ShaderType::Vertex,
            .entry       = "VSFullscreenMain",
            .source_code = source,
            .path        = m_ShaderPath,
        });
        m_PS = m_Device.CreateShader({
            .name        = "selection-outline-ps",
            .type        = gfx::ShaderType::Pixel,
            .entry       = "PSSelectionOutlineMain",
            .source_code = source,
            .path        = m_ShaderPath,
        });
    }

    m_Pipeline = m_Device.CreateRenderPipeline({
        .name                = "selection-outline",
        .shaders             = {m_VS, m_PS},
        .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
        .rasterization_state = {.cull_mode = gfx::CullMode::None},
        .render_format       = target_format,
    });
    m_TargetFormat = target_format;
}

auto passes::SelectionOutline::ImportPipeline(RenderContext& context, gfx::Format target_format) -> rg::RenderPipelineHandle {
    EnsureResources(target_format);
    return context.graph.Import(m_Pipeline);
}

auto passes::SelectionOutline::Build(
    RenderContext&                context,
    rg::TextureHandle             scene_color,
    rg::TextureHandle             scene_depth,
    const EditorSelectionBuffers& selection,
    const EditorSelectionDesc&    desc,
    rg::SamplerHandle             sampler) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!selection.Valid() ||
        !render_graph.IsValid(scene_color) ||
        !render_graph.IsValid(scene_depth) ||
        !render_graph.IsValid(selection.id) ||
        !render_graph.IsValid(selection.visual) ||
        !render_graph.IsValid(selection.depth) ||
        !render_graph.IsValid(sampler)) {
        return scene_color;
    }

    auto target_desc         = render_graph.GetResourceDesc(scene_color);
    target_desc.name         = std::pmr::string(std::format("SelectionOutlineOutput-{}", render_graph.GetFrameIndex()));
    target_desc.clear_value  = std::nullopt;
    const auto output        = render_graph.Create(target_desc);
    const auto pipeline      = ImportPipeline(context, target_desc.format);
    const auto constant      = render_graph.Create({
        .name          = "selection_outline_constant",
        .element_size  = sizeof(OutlineConstant),
        .element_count = 1,
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    const auto bindless_info = render_graph.Create({
        .name          = "selection_outline_bindless_info",
        .element_size  = sizeof(BindlessInfo),
        .element_count = 1,
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    rg::RenderPassBuilder(render_graph)
        .SetName(std::format("SelectionOutlinePass-{}", render_graph.GetFrameIndex()))
        .SetRenderTarget(output, false)
        .Read(scene_color, {}, gfx::PipelineStage::PixelShader)
        .Read(scene_depth, {}, gfx::PipelineStage::PixelShader)
        .Read(selection.id, {}, gfx::PipelineStage::PixelShader)
        .Read(selection.visual, {}, gfx::PipelineStage::PixelShader)
        .Read(selection.depth, {}, gfx::PipelineStage::PixelShader)
        .Read(constant, gfx::PipelineStage::PixelShader)
        .Read(bindless_info, gfx::PipelineStage::PixelShader)
        .AddSampler(sampler)
        .AddPipeline(pipeline)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            gfx::GPUBufferView<OutlineConstant>(pass.Resolve(constant)).front() = {
                .selected_color = desc.selected_color,
                .hovered_color  = desc.hovered_color,
                .occluded_color = desc.occluded_color,
                .params         = {
                    desc.outline_width_px,
                    desc.highlight_strength,
                    desc.show_occluded ? 1.0f : 0.0f,
                    0.015f,
                },
                .viewport       = {
                    static_cast<float>(target_desc.width),
                    static_cast<float>(target_desc.height),
                    1.0f / static_cast<float>(target_desc.width),
                    1.0f / static_cast<float>(target_desc.height),
                },
            };
            gfx::GPUBufferView<BindlessInfo>(pass.Resolve(bindless_info)).front() = {
                .outline_constant = pass.GetBindless(constant),
                .scene_color      = pass.GetBindless(scene_color),
                .scene_depth      = pass.GetBindless(scene_depth),
                .selection_id     = pass.GetBindless(selection.id),
                .selection_visual = pass.GetBindless(selection.visual),
                .selection_depth  = pass.GetBindless(selection.depth),
                .sampler          = pass.GetBindless(sampler),
            };

            auto&       cmd           = pass.GetCmd();
            const auto& render_target = pass.Resolve(output);
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
            cmd.SetPipeline(pass.Resolve(pipeline));
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info),
            });
            cmd.Draw(3);
        })
        .Finish();

    return output;
}

passes::DeferredLighting::DeferredLighting(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::DeferredLighting::EnsureResources(gfx::Format target_format) {
    if (m_Pipeline != nullptr && m_TargetFormat == target_format) return;

    const auto source = ReadShaderSource(m_ShaderPath);

    if (m_VS == nullptr) {
        m_VS = m_Device.CreateShader({
            .name        = "deferred-lighting-vs",
            .type        = gfx::ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = source,
            .path        = m_ShaderPath,
        });
        m_PS = m_Device.CreateShader({
            .name        = "deferred-lighting-ps",
            .type        = gfx::ShaderType::Pixel,
            .entry       = "PSMain",
            .source_code = source,
            .path        = m_ShaderPath,
        });
    }

    m_Pipeline = m_Device.CreateRenderPipeline({
        .name                = "deferred-lighting",
        .shaders             = {m_VS, m_PS},
        .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
        .rasterization_state = {.cull_mode = gfx::CullMode::None},
        .render_format       = target_format,
    });
    m_TargetFormat = target_format;
}

auto passes::DeferredLighting::ImportPipeline(RenderContext& context, gfx::Format target_format, std::string_view name) -> rg::RenderPipelineHandle {
    EnsureResources(target_format);
    return name.empty() ? context.graph.Import(m_Pipeline) : context.graph.Import(m_Pipeline, std::pmr::string(name));
}

auto passes::DeferredLighting::Build(
    RenderContext&           context,
    const GBufferOutput&     gbuffer,
    rg::GPUBufferHandle      frame_constant,
    rg::GPUBufferHandle      bindless_info,
    rg::SamplerHandle        sampler,
    rg::RenderPipelineHandle pipeline,
    rg::TextureHandle        target) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) ||
        !render_graph.IsValid(gbuffer.albedo) ||
        !render_graph.IsValid(gbuffer.normal) ||
        !render_graph.IsValid(gbuffer.material) ||
        !render_graph.IsValid(gbuffer.emissive) ||
        !render_graph.IsValid(frame_constant) ||
        !render_graph.IsValid(bindless_info) ||
        !render_graph.IsValid(sampler) ||
        !render_graph.IsValid(pipeline)) {
        return target;
    }

    rg::RenderPassBuilder lighting_pass_builder(render_graph);
    lighting_pass_builder
        .SetName(std::format("DeferredLightingPass-{}", render_graph.GetFrameIndex()))
        .SetRenderTarget(target, true)
        .Read(gbuffer.albedo, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.normal, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.material, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.emissive, {}, gfx::PipelineStage::PixelShader)
        .Read(frame_constant, gfx::PipelineStage::PixelShader)
        .Read(bindless_info, gfx::PipelineStage::PixelShader)
        .AddSampler(sampler)
        .AddPipeline(pipeline)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();

            gfx::GPUBufferView<BindlessInfo>(pass.Resolve(bindless_info)).front() = {
                .frame_constant   = pass.GetBindless(frame_constant),
                .gbuffer_albedo   = pass.GetBindless(gbuffer.albedo),
                .gbuffer_normal   = pass.GetBindless(gbuffer.normal),
                .gbuffer_material = pass.GetBindless(gbuffer.material),
                .gbuffer_emissive = pass.GetBindless(gbuffer.emissive),
                .sampler          = pass.GetBindless(sampler),
            };

            const auto& render_target = pass.Resolve(target);
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

            cmd.SetPipeline(pass.Resolve(pipeline));
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info),
            });
            cmd.Draw(3);
        })
        .Finish();

    return target;
}

passes::GBufferDebugView::GBufferDebugView(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::GBufferDebugView::EnsureResources(gfx::Format target_format) {
    if (m_AlbedoPipeline != nullptr &&
        m_NormalPipeline != nullptr &&
        m_MaterialPipeline != nullptr &&
        m_EmissivePipeline != nullptr &&
        m_TargetFormat == target_format) {
        return;
    }

    const auto source = ReadShaderSource(m_ShaderPath);

    if (m_VS == nullptr) {
        m_VS = m_Device.CreateShader({
            .name        = "deferred-debug-view-vs",
            .type        = gfx::ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = source,
            .path        = m_ShaderPath,
        });
    }

    auto make_pixel_shader = [&](std::string_view name, std::string_view entry) {
        return m_Device.CreateShader({
            .name        = std::pmr::string(name),
            .type        = gfx::ShaderType::Pixel,
            .entry       = std::pmr::string(entry),
            .source_code = source,
            .path        = m_ShaderPath,
        });
    };
    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& pixel_shader) {
        return m_Device.CreateRenderPipeline({
            .name                = std::pmr::string(name),
            .shaders             = {m_VS, pixel_shader},
            .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .render_format       = target_format,
        });
    };

    m_AlbedoPS   = make_pixel_shader("deferred-debug-view-albedo-ps", "PSAlbedoMain");
    m_NormalPS   = make_pixel_shader("deferred-debug-view-normal-ps", "PSNormalMain");
    m_MaterialPS = make_pixel_shader("deferred-debug-view-material-ps", "PSMaterialMain");
    m_EmissivePS = make_pixel_shader("deferred-debug-view-emissive-ps", "PSEmissiveMain");

    m_AlbedoPipeline   = make_pipeline("deferred-debug-view-albedo", m_AlbedoPS);
    m_NormalPipeline   = make_pipeline("deferred-debug-view-normal", m_NormalPS);
    m_MaterialPipeline = make_pipeline("deferred-debug-view-material", m_MaterialPS);
    m_EmissivePipeline = make_pipeline("deferred-debug-view-emissive", m_EmissivePS);
    m_TargetFormat = target_format;
}

auto passes::GBufferDebugView::Build(
    RenderContext&       context,
    const GBufferOutput& gbuffer,
    rg::SamplerHandle    sampler,
    std::string_view     view_name,
    rg::TextureHandle    target) -> rg::TextureHandle {
    auto& render_graph = context.graph;
    if (!render_graph.IsValid(target) ||
        !render_graph.IsValid(gbuffer.albedo) ||
        !render_graph.IsValid(gbuffer.normal) ||
        !render_graph.IsValid(gbuffer.material) ||
        !render_graph.IsValid(gbuffer.emissive) ||
        !render_graph.IsValid(sampler)) {
        return target;
    }

    const auto target_desc = render_graph.GetResourceDesc(target);
    EnsureResources(target_desc.format);

    auto selected_pipeline = m_AlbedoPipeline;
    if (view_name == "normal") {
        selected_pipeline = m_NormalPipeline;
    } else if (view_name == "material") {
        selected_pipeline = m_MaterialPipeline;
    } else if (view_name == "emissive") {
        selected_pipeline = m_EmissivePipeline;
    }

    const auto pipeline = render_graph.Import(selected_pipeline, "deferred_debug_view_pipeline");
    const auto bindless_info = render_graph.Create({
        .name          = "deferred_debug_view_bindless_info",
        .element_size  = sizeof(BindlessInfo),
        .element_count = 1,
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    rg::RenderPassBuilder debug_pass_builder(render_graph);
    debug_pass_builder
        .SetName(std::format("DeferredDebugViewPass-{}-{}", view_name, render_graph.GetFrameIndex()))
        .SetRenderTarget(target, true)
        .Read(gbuffer.albedo, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.normal, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.material, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.emissive, {}, gfx::PipelineStage::PixelShader)
        .Read(bindless_info, gfx::PipelineStage::PixelShader)
        .AddSampler(sampler)
        .AddPipeline(pipeline)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();

            gfx::GPUBufferView<BindlessInfo>(pass.Resolve(bindless_info)).front() = {
                .gbuffer_albedo   = pass.GetBindless(gbuffer.albedo),
                .gbuffer_normal   = pass.GetBindless(gbuffer.normal),
                .gbuffer_material = pass.GetBindless(gbuffer.material),
                .gbuffer_emissive = pass.GetBindless(gbuffer.emissive),
                .sampler          = pass.GetBindless(sampler),
            };

            const auto& render_target = pass.Resolve(target);
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

            cmd.SetPipeline(pass.Resolve(pipeline));
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info),
            });
            cmd.Draw(3);
        })
        .Finish();

    return target;
}

RenderRuntime::RenderRuntime(gfx::Device& device, const Application& app, std::string_view name)
    : RuntimeModule(std::format("RenderRuntime{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_SwapChain(device.CreateSwapChain({
          .name        = "swapchain",
          .window      = app.GetWindow(),
          .clear_color = math::Color(0, 0, 0, 1),
      })),
      m_RenderGraph(m_GfxDevice, "RenderRuntimeGraph"),
      m_GuiRenderUtils(std::make_unique<GuiRenderUtils>(m_GfxDevice)),
      m_TextRenderUtils(std::make_unique<TextRenderUtils>(m_GfxDevice, app.GetConfig().asset_root_path / "fonts")) {
    m_Clock.Start();
}

void RenderRuntime::Tick() {
    ZoneScopedN("RenderRuntimeFrame");

    if (m_App.WindowSizeChanged()) {
        m_SwapChain->Resize();
    }

    if (m_RenderGraph.IsValid(m_GuiTarget) && m_GuiDrawData != nullptr) {
        m_GuiRenderUtils->GuiPass(m_RenderGraph, m_GuiTarget, *m_GuiDrawData, m_ClearGuiTarget);
    }

    if (m_RenderGraph.Compile()) {
        m_RenderGraph.Execute();
    }

    m_SwapChain->Present();
    m_Clock.Tick();
    m_GuiTarget      = {};
    m_GuiDrawData    = nullptr;
    m_ClearGuiTarget = false;
}

auto RenderRuntime::MakeContext() noexcept -> RenderContext {
    return RenderContext{
        .device     = m_GfxDevice,
        .graph      = m_RenderGraph,
        .swap_chain = m_SwapChain,
    };
}

void RenderRuntime::RenderGui(rg::TextureHandle target, const gui::GuiDrawData& draw_data, bool clear_target) {
    m_GuiTarget      = target;
    m_GuiDrawData    = &draw_data;
    m_ClearGuiTarget = clear_target;
}

void RenderRuntime::RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target) {
    m_TextRenderUtils->TextPass(m_RenderGraph, target, commands, clear_target);
}

void RenderRuntime::CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer, gfx::TextureSubresourceLayer to_layer) {
    m_RenderGraph.QueueTextureExtraction(from, std::move(to), from_layer, to_layer);
}

void RenderRuntime::CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer) {
    m_RenderGraph.QueueBufferExtraction(from, std::move(to), from_layer);
}

void RenderRuntime::ToSwapChain(rg::TextureHandle from) {
    auto context = MakeContext();
    m_PresentPass.Build(context, from);
}

DeferredRenderer::DeferredRenderer(gfx::Device& device, const Application& app, std::string_view name)
    : IRenderer(std::format("DeferredRenderer{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_PersistentSampler(m_GfxDevice.CreateSampler({
          .name          = "sampler",
          .address_u     = gfx::AddressMode::Repeat,
          .address_v     = gfx::AddressMode::Repeat,
          .address_w     = gfx::AddressMode::Repeat,
          .mag_filter    = gfx::FilterMode::Linear,
          .min_filter    = gfx::FilterMode::Linear,
          .mipmap_filter = gfx::FilterMode::Linear,
      })),
      m_GBufferPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_gbuffer.hlsl"),
      m_DeferredLightingPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_lighting.hlsl"),
      m_GBufferDebugViewPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_debug.hlsl"),
      m_EditorSelectionMetadataPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "editor_selection_outline.hlsl"),
      m_SelectionOutlinePass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "editor_selection_outline.hlsl") {}

auto DeferredRenderer::RenderScene(RenderContext& context, const SceneView& view, rg::TextureHandle target) -> rg::TextureHandle {
    ZoneScoped;
    auto& render_graph = context.graph;
    auto  scene        = view.scene;
    const auto& camera = *view.camera;
    const auto camera_transform = view.camera_transform;

    if (scene.get() != m_CachedScene) {
        render_graph.ClearImportedResources();
        m_CachedScene = scene.get();
    }
    m_SceneDrawState.InvalidateScene();

    m_Sampler = render_graph.Import(m_PersistentSampler, "sampler");

    const auto target_desc = render_graph.GetResourceDesc(target);
    const auto albedo_pipeline = m_GBufferPass.ImportAlbedoPipeline(context);

    const auto gbuffer = m_GBufferPass.CreateTargets(
        context,
        passes::GBuffer::Desc{
            .width  = target_desc.width,
            .height = target_desc.height,
        });

    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;

    const auto view_matrix = math::look_at(global_eye, global_look_dir, global_up);
    const auto projection = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto proj_view  = projection * view_matrix;
    const auto frustum    = math::extract_frustum(proj_view);

    for (const auto entity : scene->GetMeshEntities()) {
        const auto mesh      = entity.Get<asset::MeshComponent>().mesh;
        const auto transform = entity.Get<asset::Transform>().world_matrix;

        if (mesh->aabb.Valid()) {
            auto world_aabb = math::transform_aabb(transform, mesh->aabb);
            if (!math::is_aabb_visible(frustum, world_aabb)) continue;
        }

        RecordInstance(render_graph, entity, mesh, transform);
    }
    UpdateConstantBuffer(render_graph, albedo_pipeline);

    SceneFrameConstant frame_constant{};
    {
        const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
        const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
        const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;

        frame_constant.camera_pos     = {global_eye, 1.0f},
        frame_constant.view           = math::look_at(global_eye, global_look_dir, global_up);
        frame_constant.projection     = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
        frame_constant.proj_view      = frame_constant.projection * frame_constant.view,
        frame_constant.inv_view       = math::inverse(frame_constant.view);
        frame_constant.inv_projection = math::inverse(frame_constant.projection);
        frame_constant.inv_proj_view  = math::inverse(frame_constant.proj_view);

        frame_constant.ambient_intensity = 0.62f;
        frame_constant.exposure          = 1.05f;
        frame_constant.ssao_strength     = 0.55f;
        frame_constant.viewport          = {
            static_cast<float>(target_desc.width),
            static_cast<float>(target_desc.height),
            1.0f / static_cast<float>(target_desc.width),
            1.0f / static_cast<float>(target_desc.height),
        };

        auto add_light = [&](math::vec3f world_position, math::vec3f color, float intensity) {
            if (frame_constant.light_count >= MaxDeferredLights) return;

            const auto light_index      = frame_constant.light_count++;
            const auto position         = math::vec4f(world_position, 1.0f);
            const auto position_in_view = frame_constant.view * position;

            frame_constant.lights[light_index] = {
                .position_in_view = position_in_view,
                .color_intensity  = {color, intensity},
            };

            if (light_index == 0) {
                frame_constant.light_position    = position;
                frame_constant.light_pos_in_view = position_in_view;
                frame_constant.light_color       = color;
                frame_constant.light_intensity   = intensity;
            }
        };

        for (const auto entity : scene->GetLightEntities()) {
            const auto light = entity.Get<asset::LightComponent>().light;
            if (!light) continue;

            const auto light_transform = entity.Get<asset::Transform>().world_matrix;
            add_light(light_transform.col(3).xyz, light->parameters.color.rgb, std::max(light->parameters.intensity, 1.0f) * 0.18f);
        }

        if (frame_constant.light_count == 0) {
            const auto forward = math::normalize(global_look_dir);
            const auto right   = math::normalize(math::cross(forward, global_up));
            add_light(global_eye - forward * 2.0f + global_up * 1.0f, math::Color::White().rgb, 8.0f);
            add_light(global_eye - forward * 1.5f + right * 3.0f, math::Color{1.0f, 0.78f, 0.55f, 1.0f}.rgb, 4.0f);
            add_light(global_eye - forward * 1.5f - right * 3.0f, math::Color{0.55f, 0.72f, 1.0f, 1.0f}.rgb, 4.0f);
        }
    };

    m_GBufferPass.BuildAlbedoPass(
        context,
        m_SceneDrawState,
        passes::GBuffer::AlbedoPassDesc{
            .pass_name                = std::pmr::string(std::format("DeferredGBufferAlbedoPass-{}", render_graph.GetFrameIndex())),
            .target                   = gbuffer.albedo,
            .depth                    = gbuffer.depth,
            .frame_constant           = frame_constant,
            .frame_constant_buffer    = m_FrameConstantBuffer,
            .instance_constant_buffer = m_InstanceConstantBuffer,
            .bindless_info_buffer     = m_BindlessInfoConstantBuffer,
            .sampler                  = m_Sampler,
            .device_type              = m_GfxDevice.device_type,
            .width                    = target_desc.width,
            .height                   = target_desc.height,
        });

    const auto normal_pipeline = m_GBufferPass.ImportNormalPipeline(context);
    std::size_t num_draws = 0;
    for (const auto& instance_info : m_SceneDrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_NormalBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "normal_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    // The albedo pass uploads per-frame, instance, and material constants;
    // dependent G-buffer passes read its output to stay in a later recording layer.
    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferNormalPass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.normal,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.albedo,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_NormalBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = normal_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
        });

    const auto selection_buffers = m_EditorSelectionMetadataPass.Build(
        context,
        m_SceneDrawState,
        m_FrameConstantBuffer,
        view.editor_selection,
        target_desc.width,
        target_desc.height);

    const auto material_pipeline = m_GBufferPass.ImportMaterialPipeline(context);

    m_MaterialBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "material_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferMaterialPass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.material,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.normal,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_MaterialBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = material_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
        });

    const auto emissive_pipeline = m_GBufferPass.ImportEmissivePipeline(context);

    m_EmissiveBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "emissive_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferEmissivePass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.emissive,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.albedo,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_EmissiveBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = emissive_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
        });

    const auto debug_view = std::getenv("HITAGI_RENDER_DEBUG_VIEW");
    if (debug_view != nullptr && std::string_view(debug_view) != "final") {
        m_GBufferDebugViewPass.Build(
            context,
            gbuffer,
            m_Sampler,
            debug_view,
            target);
        return target;
    }

    const auto lighting_pipeline = m_DeferredLightingPass.ImportPipeline(context, target_desc.format);

    m_DeferredLightingBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "deferred_lighting_bindless_info",
        .element_size  = sizeof(passes::DeferredLighting::BindlessInfo),
        .element_count = 1,
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    auto scene_color = m_DeferredLightingPass.Build(
        context,
        gbuffer,
        m_FrameConstantBuffer,
        m_DeferredLightingBindlessInfoConstantBuffer,
        m_Sampler,
        lighting_pipeline,
        target);

    if (selection_buffers.Valid()) {
        scene_color = m_SelectionOutlinePass.Build(
            context,
            scene_color,
            gbuffer.albedo,
            selection_buffers,
            view.editor_selection,
            m_Sampler);
    }
    return scene_color;
}

void DeferredRenderer::RecordMaterialInstance(rg::RenderGraph& render_graph, const std::shared_ptr<asset::MaterialInstance>& material_instance) {
    if (!material_instance) return;
    if (!material_instance->GetMaterial()) {
        spdlog::warn("Material instance '{}' has no material assigned, skipping", material_instance->GetName());
        return;
    }
    auto [it, inserted] = m_SceneDrawState.material_instance_infos.try_emplace(
        material_instance.get(),
        MaterialInstanceInfo{
            .material_instance = material_instance,
        });
    auto& info = it->second;

    if (inserted) {
        auto associated = material_instance->GetAssociatedTextures();
        for (std::size_t ti = 0; ti < associated.size(); ti++) {
            const auto& texture = associated[ti];
            if (texture && !texture->Empty()) {
                texture->InitGPUData(m_GfxDevice);
                info.textures.emplace_back(render_graph.Import(texture->GetGPUData(), texture->GetUniqueName()));
            } else {
                info.textures.emplace_back(rg::TextureHandle{});
            }
        }
    }

    m_SceneDrawState.active_material_instances.emplace(material_instance.get());
}

void DeferredRenderer::RecordMesh(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Mesh>& mesh) {
    auto [it, inserted] = m_SceneDrawState.mesh_infos.try_emplace(mesh.get(), MeshInfo{.mesh = mesh});
    auto& info          = it->second;

    if (inserted) {
        mesh->vertices->InitGPUData(m_GfxDevice);
        mesh->indices->InitGPUData(m_GfxDevice);

        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            auto attr_data = mesh->vertices->GetAttributeData(attr);
            if (!attr_data.has_value()) return;
            info.vertices[attr] = render_graph.Import(attr_data->get().gpu_buffer);
        });

        info.indices = render_graph.Import(mesh->indices->GetIndexData().gpu_buffer);
    }

    for (const auto& sub_mesh : mesh->sub_meshes) {
        RecordMaterialInstance(render_graph, sub_mesh.material_instance);
    }
}

void DeferredRenderer::RecordInstance(rg::RenderGraph& render_graph, ecs::Entity entity, const std::shared_ptr<asset::Mesh>& mesh, math::mat4f transform) {
    RecordMesh(render_graph, mesh);
    m_SceneDrawState.instance_infos.emplace_back(InstanceInfo{
        .entity        = entity,
        .mesh          = mesh,
        .instance_data = {
            .model = transform,
        },
        .instance_index = m_SceneDrawState.instance_infos.size(),
    });
}

void DeferredRenderer::UpdateConstantBuffer(rg::RenderGraph& render_graph, rg::RenderPipelineHandle albedo_pipeline) {
    std::pmr::unordered_map<std::shared_ptr<asset::Material>, std::size_t> material_instance_counter;
    m_SceneDrawState.material_instance_indices.clear();
    for (auto* const material_instance : m_SceneDrawState.active_material_instances) {
        const auto material = material_instance->GetMaterial();
        m_SceneDrawState.material_instance_indices.emplace(material_instance, material_instance_counter[material]++);
    }

    for (const auto& [material, num_instances] : material_instance_counter) {
        auto pipeline_it = m_SceneDrawState.pipeline_handles.find(material.get());
        if (pipeline_it == m_SceneDrawState.pipeline_handles.end()) {
            pipeline_it = m_SceneDrawState.pipeline_handles.emplace(material.get(), albedo_pipeline).first;
        }
        auto pipeline_handle = pipeline_it->second;

        auto constant_handle = render_graph.Create(
            {
                .name          = std::pmr::string(material->GetName()),
                .element_size  = material->CalculateMaterialBufferSize(m_GfxDevice.device_type == gfx::Device::Type::DX12),
                .element_count = num_instances,  // we not use material->GetNumInstances for reduce memory usage
                .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            },
            material->GetName());

        m_SceneDrawState.material_infos.emplace(
            material.get(),
            MaterialInfo{
                .material          = material,
                .pipeline          = pipeline_handle,
                .material_constant = constant_handle,
            });
    }

    m_InstanceConstantBuffer = render_graph.Create({
        .name          = "instance_constant",
        .element_size  = sizeof(SceneInstanceConstant),
        .element_count = std::max<std::size_t>(1, m_SceneDrawState.instance_infos.size()),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_FrameConstantBuffer = render_graph.Create(
        {
            .name          = "frame_constant",
            .element_size  = sizeof(SceneFrameConstant),
            .element_count = 1,
            .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
        });

    std::size_t num_draws = 0;
    for (const auto& instance_info : m_SceneDrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_BindlessInfoConstantBuffer = render_graph.Create({
        .name          = "bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
}
void DeferredRenderer::ClearFrameState() {
    m_Sampler                                     = {};
    m_FrameConstantBuffer                         = {};
    m_InstanceConstantBuffer                      = {};
    m_BindlessInfoConstantBuffer                  = {};
    m_NormalBindlessInfoConstantBuffer            = {};
    m_MaterialBindlessInfoConstantBuffer          = {};
    m_EmissiveBindlessInfoConstantBuffer          = {};
    m_DeferredLightingBindlessInfoConstantBuffer = {};
    m_GBufferDebugViewBindlessInfoConstantBuffer = {};

    m_SceneDrawState.ClearFrame();
}

}  // namespace hitagi::render
