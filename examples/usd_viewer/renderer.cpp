module;
#include <cstddef>
#include "interop/tracy_macros.hpp"
module anime_viewer;
import std;
import interop.tracy;

namespace game::anime_viewer {
ToonRenderer::ToonRenderer(asset::ResourceLoadContext resources) : m_Resources(resources) {
    m_Fullscreen = gfx::RenderPipeline::Create(
        resources.device,
        resources.bindings,
        {
            .name                = "Anime fullscreen",
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .render_format       = gfx::Format::R8G8B8A8_UNORM,
        },
        {
            gfx::Shader::Create(
                resources.device,
                resources.shader_compiler,
                {
                    .name    = "FullscreenVS",
                    .type    = gfx::ShaderType::Vertex,
                    .entry   = "FullscreenVS",
                    .program = AnimeShaderProgram(),
                }),
            gfx::Shader::Create(
                resources.device,
                resources.shader_compiler,
                {
                    .name    = "FullscreenPS",
                    .type    = gfx::ShaderType::Pixel,
                    .entry   = "FullscreenPS",
                    .program = AnimeShaderProgram(),
                }),
        });
    m_Sampler = gfx::Sampler::Create(
        resources.device,
        resources.bindings,
        {
            .name       = "Anime linear sampler",
            .address_u  = gfx::AddressMode::Clamp,
            .address_v  = gfx::AddressMode::Clamp,
            .mag_filter = gfx::FilterMode::Linear,
            .min_filter = gfx::FilterMode::Linear,
        });
    m_FrameBuffer = gfx::GPUBuffer::Create(
        resources.device,
        {
            .name   = "Anime frame",
            .size   = utils::align(sizeof(AnimeFrameData), gfx::GPUBuffer::GetStorageViewRequirements(resources.device).size_alignment),
            .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
        });
}

auto ToonRenderer::Build(rg::RenderGraph& graph, std::span<const render::RenderDrawItem> items,
                         const render::RenderView& view, std::uint32_t width, std::uint32_t height,
                         math::vec3f light, float outline, math::vec3f bounds_min, math::vec3f bounds_max,
                         RenderSettings settings, bool diagnostics) -> FrameImages {
    ZoneScopedN("Anime BuildGraph");
    FrameImages images;
    const auto  vp        = view.projection * view.view;
    light                 = math::normalize(light);
    const auto light_eye  = (bounds_min + bounds_max) * 0.5f + light * 5.0f;
    const auto light_view = math::look_at(light_eye, -light, math::vec3f{0, 1, 0});
    // Fit the static character's bounds in light space. Camera zoom does not
    // change the projection, so the shadow texel grid remains stable.
    math::vec3f light_min{std::numeric_limits<float>::max()}, light_max{std::numeric_limits<float>::lowest()};
    for (unsigned corner = 0; corner < 8; ++corner) {
        const auto p =
            light_view * math::vec4f{
                             corner & 1 ? bounds_max.x : bounds_min.x,
                             corner & 2 ? bounds_max.y : bounds_min.y,
                             corner & 4 ? bounds_max.z : bounds_min.z,
                             1,
                         };
        for (unsigned axis = 0; axis < 3; ++axis) {
            light_min[axis] = std::min(light_min[axis], p[axis]);
            light_max[axis] = std::max(light_max[axis], p[axis]);
        }
    }
    constexpr std::uint32_t shadow_size      = 2048;
    const auto              shadow_extent    = std::max(light_max.x - light_min.x, light_max.y - light_min.y) + 0.08f;
    const float             shadow_texel     = shadow_extent / shadow_size;
    const float             shadow_x         = std::round((light_min.x + light_max.x) * 0.5f / shadow_texel) * shadow_texel;
    const float             shadow_y         = std::round((light_min.y + light_max.y) * 0.5f / shadow_texel) * shadow_texel;
    auto                    light_projection = math::ortho(shadow_x - shadow_extent * 0.5f, shadow_x + shadow_extent * 0.5f,
                                                           shadow_y - shadow_extent * 0.5f, shadow_y + shadow_extent * 0.5f,
                                                           std::max(0.01f, -light_max.z - 0.1f), -light_min.z + 4.0f);
    light_projection[2]                      = (light_projection[2] + light_projection[3]) * 0.5f;
    const auto        light_vp               = light_projection * light_view;
    const auto        render_width = width * settings.scale, render_height = height * settings.scale;
    const math::vec4f shadow_settings{static_cast<float>(shadow_size), shadow_texel, 0.003f, 0};
    const auto        storage_requirements = gfx::GPUBuffer::GetStorageViewRequirements(m_Resources.device);
    const auto        data_buffer          = [&](std::string_view name, std::size_t size) {
        return graph.Create(gfx::GPUBufferDesc{
            .name   = std::pmr::string(name),
            .size   = utils::align(std::max<std::size_t>(4, size), storage_requirements.size_alignment),
            .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
        });
    };
    const AnimeFrameData frame_data{
        .inverse_vp      = math::inverse(vp),
        .shadow_mvp      = light_vp,
        .eye             = math::vec4f(view.camera_position, 1),
        .light           = math::vec4f(light, 0),
        .shadow_settings = shadow_settings,
        .options         = {settings.shadows ? 1.0f : 0.0f, settings.face_lighting ? 1.0f : 0.0f,
                            settings.specular ? 1.0f : 0.0f, settings.asset_controls ? 1.0f : 0.0f},
    };
    // RenderRuntime completes the previous graph before Build runs again.
    // Upload once before command recording; every pass reads the same snapshot.
    std::memcpy(m_FrameBuffer->Map(), &frame_data, sizeof(frame_data));
    m_FrameBuffer->UnMap();
    const auto frame_buffer = graph.Import(m_FrameBuffer);
    struct Draw {
        std::array<rg::GPUBufferHandle, 3> vertices;
        rg::GPUBufferHandle                indices;
        gfx::Format                        index_format;
        asset::Mesh::SubMesh               submesh;
        math::mat4f                        model;
    };
    std::vector<Draw> draws;
    for (const auto& item : items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        item.mesh->Load(m_Resources);
        std::array<rg::GPUBufferHandle, 3> vertices;
        std::size_t                        binding = 0;
        for (auto attr : {asset::VertexAttribute::Position, asset::VertexAttribute::Normal, asset::VertexAttribute::UV0}) {
            const auto data = item.mesh->vertices->GetAttributeData(attr);
            if (!data || !data->get().gpu_buffer) throw std::runtime_error("Mesh is missing position, normal or UV data");
            vertices[binding++] = graph.Import(data->get().gpu_buffer);
        }
        for (const auto& submesh : item.mesh->sub_meshes) {
            if (!submesh.material) continue;
            submesh.material->Load(m_Resources);
            draws.push_back({
                .vertices     = vertices,
                .indices      = graph.Import(item.mesh->indices->GetGPUData()),
                .index_format = item.mesh->indices->Type() == asset::IndexType::UINT16 ? gfx::Format::R16_UINT : gfx::Format::R32_UINT,
                .submesh      = submesh,
                .model        = item.transform,
            });
        }
    }
    if (draws.empty()) throw std::runtime_error("USD stage contains no renderable triangles");
    images.draws              = draws.size();
    const auto        sampler = graph.Import(m_Sampler);
    rg::TextureHandle shadow_map, hair_shadow_map;
    auto              geometry = [&](
                                     std::string_view name, std::string_view contract, std::uint32_t mode,
                                     const math::mat4f& matrix, std::uint32_t w, std::uint32_t h) {
        ZoneScopedN("Anime BuildGeometryPass");
        ZoneText(name.data(), name.size());
        const auto color = graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(name),
            .width       = w,
            .height      = h,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0, 0, 0, 0},
            .usages      = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::RenderTarget,
        });
        const auto depth = graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(name) + " depth",
            .width       = w,
            .height      = h,
            .format      = gfx::Format::D32_FLOAT,
            .clear_value = gfx::ClearDepthStencil{.depth = 1},
            .usages      = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::DepthStencil,
        });

        rg::RenderPassBuilder builder(graph);
        builder.SetName(name);
        builder.SetRenderTarget(color, true);
        builder.SetDepthStencil(depth, true);
        const auto frame_access = builder.Read(
            frame_buffer,
            {.element_size  = sizeof(AnimeFrameData),
             .element_count = 1});
        const auto pass_access     = builder.Read(data_buffer("Anime pass", sizeof(AnimePassData)), {.element_size = sizeof(AnimePassData), .element_count = 1});
        const auto draw_access     = builder.Read(data_buffer("Anime draw bindings", sizeof(AnimeDrawBindings) * draws.size()),
                                                  {.element_size = sizeof(AnimeDrawBindings), .element_count = draws.size()});
        const auto instance_access = builder.Read(data_buffer("Anime instances", sizeof(AnimeInstanceData) * draws.size()),
                                                  {.element_size = sizeof(AnimeInstanceData), .element_count = draws.size()});
        builder.AddSampler(sampler);
        rg::TextureEdgeHandle shadow_access, hair_shadow_access;
        if (mode >= 3) {
            shadow_access      = builder.Read(shadow_map);
            hair_shadow_access = builder.Read(hair_shadow_map);
        }
        struct Record {
            Draw                                 draw;
            std::shared_ptr<gfx::RenderPipeline> pipeline;
            // draw.submesh.material owns this pass through graph execution.
            const asset::MaterialPass*             material_pass;
            std::uint32_t                          material_offset;
            std::array<rg::GPUBufferEdgeHandle, 3> vertices;
            rg::GPUBufferEdgeHandle                indices;
        };
        std::vector<Record> records;
        records.reserve(draws.size());
        struct MeshAccess {
            std::array<rg::GPUBufferHandle, 3>     buffers;
            rg::GPUBufferHandle                    index_buffer;
            std::array<rg::GPUBufferEdgeHandle, 3> vertices;
            rg::GPUBufferEdgeHandle                indices;
        };
        std::vector<MeshAccess>                    meshes;
        std::vector<std::shared_ptr<gfx::TextureView>> texture_views;
        std::size_t                                material_size = 0;
        for (const auto& draw : draws) {
            const auto& material      = *draw.submesh.material;
            const auto* material_pass = material.FindPass(contract);
            if (!material_pass || !material_pass->pipeline) continue;
            auto pipeline = material_pass->pipeline->GetBuiltPipeline();
            if (!pipeline) continue;
            // One byte-addressed material buffer per pass. Each record selects
            // its own packed layout; only 32-bit load alignment is required.
            material_size        = utils::align(material_size, std::size_t{4});
            const auto data_size = material_pass->material_data.GetDataSize();
            if (material_size > std::numeric_limits<std::uint32_t>::max() ||
                data_size > std::numeric_limits<std::uint32_t>::max() - material_size) {
                throw std::overflow_error("Anime material buffer exceeds shader address range");
            }
            auto& record = records.emplace_back(Record{
                .draw            = draw,
                .pipeline        = std::move(pipeline),
                .material_pass   = material_pass,
                .material_offset = static_cast<std::uint32_t>(material_size),
            });
            material_size += data_size;
            auto mesh = std::ranges::find_if(meshes, [&](const MeshAccess& access) {
                return access.buffers == draw.vertices && access.index_buffer == draw.indices;
            });
            if (mesh == meshes.end()) {
                MeshAccess access{.buffers = draw.vertices, .index_buffer = draw.indices};
                for (std::size_t b = 0; b < draw.vertices.size(); ++b)
                    access.vertices[b] = builder.ReadAsVertices(draw.vertices[b]);
                access.indices = builder.ReadAsIndices(draw.indices);
                meshes.push_back(access);
                mesh = std::prev(meshes.end());
            }
            record.vertices = mesh->vertices;
            record.indices  = mesh->indices;
            for (const auto& view : AnimeMaterialViews(material, *material_pass)) {
                if (std::ranges::find(texture_views, view) != texture_views.end()) continue;
                texture_views.push_back(view);
                builder.Read(view);
            }
        }
        const auto material_access = builder.Read(data_buffer("Anime materials", material_size));
        if (contract == kAnimeForward && mode == 3) images.draws = records.size();
        builder.SetExecutor([=, records = std::move(records)](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            AnimePassData pass_data{.mode = mode, .sampler = pass.Resolve(sampler).GetBindlessHandle()};
            if (mode >= 3) {
                pass_data.shadow      = pass.Resolve(shadow_access).GetBindlessHandle();
                pass_data.hair_shadow = pass.Resolve(hair_shadow_access).GetBindlessHandle();
            }
            pass.Resolve(pass_access).GetMappedSpan<AnimePassData>().front() = pass_data;
            auto       instances                                             = pass.Resolve(instance_access).GetMappedSpan<AnimeInstanceData>();
            auto       bindings                                              = pass.Resolve(draw_access).GetMappedSpan<AnimeDrawBindings>();
            auto       material_bytes                                        = pass.Resolve(material_access).GetMappedSpan<std::byte>();
            const auto frame_handle                                          = pass.Resolve(frame_access).GetBindlessHandle();
            const auto instance_handle                                       = pass.Resolve(instance_access).GetBindlessHandle();
            const auto material_handle                                       = pass.Resolve(material_access).GetBindlessHandle();
            const auto pass_handle                                           = pass.Resolve(pass_access).GetBindlessHandle();
            const auto draw_handle                                           = pass.Resolve(draw_access).GetBindlessHandle();

            auto& cmd = pass.GetCmd();
            cmd.SetViewPort({.width = static_cast<float>(w), .height = static_cast<float>(h)});
            cmd.SetScissorRect({.width = w, .height = h});

            for (std::size_t i = 0; i < records.size(); ++i) {
                const auto& record        = records[i];
                const auto& draw          = record.draw;
                const auto& material_data = record.material_pass->material_data;
                if (!material_data.Empty())
                    std::memcpy(material_bytes.data() + record.material_offset, material_data.GetData(), material_data.GetDataSize());
                instances[i] = {
                    .mvp           = matrix * draw.model,
                    .model         = draw.model,
                    .normal_matrix = math::transpose(math::inverse(draw.model)),
                };
                bindings[i] = {
                    .frame           = frame_handle,
                    .instance        = instance_handle,
                    .material_data   = material_handle,
                    .pass_data       = pass_handle,
                    .instance_index  = static_cast<std::uint32_t>(i),
                    .instance_stride = sizeof(AnimeInstanceData),
                    .material_offset = record.material_offset,
                };
                cmd.SetPipeline(*record.pipeline);
                cmd.PushBindlessMetaInfo({
                    .handle        = draw_handle,
                    .record_index  = static_cast<std::uint32_t>(i),
                    .record_stride = sizeof(AnimeDrawBindings),
                });

                const std::array<std::reference_wrapper<const gfx::GPUBuffer>, 3> vertices{
                    *pass.Resolve(record.vertices[0]).GetDesc().buffer,
                    *pass.Resolve(record.vertices[1]).GetDesc().buffer,
                    *pass.Resolve(record.vertices[2]).GetDesc().buffer,
                };
                constexpr std::array<std::size_t, 3> offsets{};
                cmd.SetVertexBuffers(0, vertices, offsets);
                cmd.SetIndexBuffer(*pass.Resolve(record.indices).GetDesc().buffer, 0, draw.index_format);
                cmd.DrawIndexed(draw.submesh.index_count, 1, draw.submesh.index_offset, draw.submesh.vertex_offset);
            }
        });
        builder.Finish();
        images.stages[std::string(name)] = color;
        return std::pair{color, depth};
    };

    auto [shadow_color, shadow]   = geometry("shadow", kAnimeShadowCaster, 2, light_vp, shadow_size, shadow_size);
    auto [hair_color, hair_depth] = geometry("hair_shadow", kAnimeHairShadowCaster, 2, light_vp, shadow_size, shadow_size);
    shadow_map                    = shadow;
    hair_shadow_map               = hair_depth;
    auto [albedo, depth]          = geometry("albedo", kAnimeAlbedo, 0, vp, render_width, render_height);
    auto [normals, normal_depth]  = geometry("normals", kAnimeNormals, 1, vp, render_width, render_height);
    auto [toon, toon_depth]       = geometry("toon", kAnimeForward, 3, vp, render_width, render_height);
    if (diagnostics) {
        geometry("material_id", kAnimeForward, 4, vp, render_width, render_height);
        geometry("shadow_factor", kAnimeForward, 5, vp, render_width, render_height);
        geometry("lighting_band", kAnimeForward, 6, vp, render_width, render_height);
        geometry("specular", kAnimeForward, 7, vp, render_width, render_height);
    }
    images.stages["depth"]             = depth;
    images.stages["shadow_depth"]      = shadow;
    images.stages["hair_shadow_depth"] = hair_depth;

    auto fullscreen = [&](std::string_view name, int mode, std::array<rg::TextureHandle, 4> inputs, bool resolve = false) {
        ZoneScopedN("Anime BuildFullscreenPass");
        ZoneText(name.data(), name.size());
        const auto            w = resolve ? width : render_width, h = resolve ? height : render_height;
        const auto            color = graph.Create(gfx::TextureDesc{
            .name        = std::pmr::string(name),
            .width       = w,
            .height      = h,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0, 0, 0, 0},
            .usages      = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::RenderTarget,
        });
        rg::RenderPassBuilder builder(graph);
        builder.SetName(name);
        builder.SetRenderTarget(color, true);
        const auto                           data = builder.Read(data_buffer("Anime fullscreen", sizeof(AnimeFullscreenData)), {.element_size = sizeof(AnimeFullscreenData), .element_count = 1});
        std::array<rg::TextureEdgeHandle, 4> accesses;
        for (std::size_t i = 0; i < 4; ++i) accesses[i] = builder.Read(inputs[i]);
        builder.AddSampler(sampler);
        const auto frame_access = builder.Read(frame_buffer, {.element_size = sizeof(AnimeFrameData), .element_count = 1});
        builder.SetExecutor([=, this](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            pass.Resolve(data).GetMappedSpan<AnimeFullscreenData>().front() = {
                .frame    = pass.Resolve(frame_access).GetBindlessHandle(),
                .settings = {static_cast<float>(mode), static_cast<float>(w), static_cast<float>(h), outline * settings.scale},
                .scale    = static_cast<float>(settings.scale),
                .texture0 = pass.Resolve(accesses[0]).GetBindlessHandle(),
                .texture1 = pass.Resolve(accesses[1]).GetBindlessHandle(),
                .texture2 = pass.Resolve(accesses[2]).GetBindlessHandle(),
                .texture3 = pass.Resolve(accesses[3]).GetBindlessHandle(),
                .sampler  = pass.Resolve(sampler).GetBindlessHandle(),
            };
            auto& cmd = pass.GetCmd();
            cmd.SetViewPort({.width = static_cast<float>(w), .height = static_cast<float>(h)});
            cmd.SetScissorRect({.width = w, .height = h});
            cmd.SetPipeline(*m_Fullscreen);
            cmd.PushBindlessMetaInfo({.handle = pass.Resolve(data).GetBindlessHandle(), .record_stride = sizeof(AnimeFullscreenData)});
            cmd.Draw(3);
        });
        builder.Finish();
        images.stages[std::string(name)] = color;
        return color;
    };

    const auto world     = fullscreen("world", 3, {shadow, shadow, shadow, shadow});
    const auto composite = fullscreen("outline", 5, {toon, world, depth, normals});
    images.final         = fullscreen("final", 6, {composite, toon, normals, normals}, true);
    return images;
}
}  // namespace game::anime_viewer
