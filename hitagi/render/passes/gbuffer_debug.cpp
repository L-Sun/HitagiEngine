module render;
import std;

namespace hitagi::render {

passes::GBufferDebugView::GBufferDebugView(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, ShaderSource shader)
    : m_Device(device), m_Bindings(bindings), m_ShaderCompiler(compiler), m_Shader(std::move(shader)) {}

void passes::GBufferDebugView::EnsureResources(gfx::Format target_format) {
    if (m_AlbedoPipeline != nullptr &&
        m_NormalPipeline != nullptr &&
        m_MaterialPipeline != nullptr &&
        m_EmissivePipeline != nullptr &&
        m_ObjectMaterialIdPipeline != nullptr &&
        m_TargetFormat == target_format) {
        return;
    }

    if (m_VS == nullptr) {
        m_VS = hitagi::gfx::Shader::Create(m_Device, m_ShaderCompiler, {
                                                                           .name        = "deferred-debug-view-vs",
                                                                           .type        = gfx::ShaderType::Vertex,
                                                                           .entry       = "VSMain",
                                                                           .source_code = m_Shader.code,
                                                                           .path        = m_Shader.path,
                                                                       });
    }

    auto make_pixel_shader = [&](std::string_view name, std::string_view entry) {
        return hitagi::gfx::Shader::Create(m_Device, m_ShaderCompiler, {
                                                                           .name        = std::pmr::string(name),
                                                                           .type        = gfx::ShaderType::Pixel,
                                                                           .entry       = std::pmr::string(entry),
                                                                           .source_code = m_Shader.code,
                                                                           .path        = m_Shader.path,
                                                                       });
    };
    auto make_pipeline = [&](std::string_view name, const std::shared_ptr<gfx::Shader>& pixel_shader) {
        return hitagi::gfx::RenderPipeline::Create(m_Device, m_Bindings,
                                                   {
                                                       .name                = std::pmr::string(name),
                                                       .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
                                                       .rasterization_state = {.cull_mode = gfx::CullMode::None},
                                                       .render_format       = target_format,
                                                   },
                                                   {m_VS, pixel_shader});
    };

    m_AlbedoPS           = make_pixel_shader("deferred-debug-view-albedo-ps", "PSAlbedoMain");
    m_NormalPS           = make_pixel_shader("deferred-debug-view-normal-ps", "PSNormalMain");
    m_MaterialPS         = make_pixel_shader("deferred-debug-view-material-ps", "PSMaterialMain");
    m_EmissivePS         = make_pixel_shader("deferred-debug-view-emissive-ps", "PSEmissiveMain");
    m_ObjectMaterialIdPS = make_pixel_shader("deferred-debug-view-object-material-id-ps", "PSObjectMaterialIdMain");

    m_AlbedoPipeline           = make_pipeline("deferred-debug-view-albedo", m_AlbedoPS);
    m_NormalPipeline           = make_pipeline("deferred-debug-view-normal", m_NormalPS);
    m_MaterialPipeline         = make_pipeline("deferred-debug-view-material", m_MaterialPS);
    m_EmissivePipeline         = make_pipeline("deferred-debug-view-emissive", m_EmissivePS);
    m_ObjectMaterialIdPipeline = make_pipeline("deferred-debug-view-object-material-id", m_ObjectMaterialIdPS);
    m_TargetFormat             = target_format;
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
        !render_graph.IsValid(gbuffer.object_material_id) ||
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
    } else if (view_name == "material_id" || view_name == "object_id") {
        selected_pipeline = m_ObjectMaterialIdPipeline;
    } else if (view_name == "emissive") {
        selected_pipeline = m_EmissivePipeline;
    }

    const auto bindless_info = render_graph.Create({
        .name   = "deferred_debug_view_bindless_info",
        .size   = utils::align(sizeof(BindlessInfo), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    rg::RenderPassBuilder debug_pass_builder(render_graph);
    debug_pass_builder.SetName(std::format("DeferredDebugViewPass-{}-{}", view_name, render_graph.GetFrameIndex()));
    debug_pass_builder.SetRenderTarget(target, true);
    const auto albedo_access             = debug_pass_builder.Read(gbuffer.albedo, {}, gfx::PipelineStage::PixelShader);
    const auto normal_access             = debug_pass_builder.Read(gbuffer.normal, {}, gfx::PipelineStage::PixelShader);
    const auto material_access           = debug_pass_builder.Read(gbuffer.material, {}, gfx::PipelineStage::PixelShader);
    const auto emissive_access           = debug_pass_builder.Read(gbuffer.emissive, {}, gfx::PipelineStage::PixelShader);
    const auto object_material_id_access = debug_pass_builder.Read(gbuffer.object_material_id, {}, gfx::PipelineStage::PixelShader);
    const auto bindless_info_access      = debug_pass_builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(BindlessInfo), .element_count = 1}, gfx::PipelineStage::PixelShader);
    debug_pass_builder.AddSampler(sampler);
    debug_pass_builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        pass.Resolve(bindless_info_access).GetMappedSpan<BindlessInfo>().front() = {
            .gbuffer_albedo     = pass.Resolve(albedo_access).GetBindlessHandle(),
            .gbuffer_normal     = pass.Resolve(normal_access).GetBindlessHandle(),
            .gbuffer_material   = pass.Resolve(material_access).GetBindlessHandle(),
            .gbuffer_emissive   = pass.Resolve(emissive_access).GetBindlessHandle(),
            .object_material_id = pass.Resolve(object_material_id_access).GetBindlessHandle(),
            .sampler            = pass.Resolve(sampler).GetBindlessHandle(),
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

        cmd.SetPipeline(*selected_pipeline);
        cmd.PushBindlessMetaInfo({
            .handle = pass.Resolve(bindless_info_access).GetBindlessHandle(),
        });
        cmd.Draw(3);
    });
    debug_pass_builder.Finish();

    return target;
}

}  // namespace hitagi::render
