module;


module render;
import std;

namespace hitagi::render {

namespace {

auto ReadShaderSource(const std::filesystem::path& path) -> std::pmr::string {
    if (core::FileIOManager::Get() == nullptr) return {};
    return std::pmr::string(core::FileIOManager::Get()->SyncOpenAndReadBinary(path).Str());
}

}  // namespace
passes::GBufferDebugView::GBufferDebugView(gfx::Device& device, std::filesystem::path shader_path)
    : m_Device(device),
      m_ShaderPath(std::move(shader_path)) {}

void passes::GBufferDebugView::EnsureResources(gfx::Format target_format) {
    if (m_AlbedoPipeline != nullptr &&
        m_NormalPipeline != nullptr &&
        m_MaterialPipeline != nullptr &&
        m_EmissivePipeline != nullptr &&
        m_ObjectMaterialIdPipeline != nullptr &&
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
        return m_Device.CreateRenderPipeline(
            {
                .name                = std::pmr::string(name),
                .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
                .rasterization_state = {.cull_mode = gfx::CullMode::None},
                .render_format       = target_format,
            },
            {m_VS, pixel_shader});
    };

    m_AlbedoPS   = make_pixel_shader("deferred-debug-view-albedo-ps", "PSAlbedoMain");
    m_NormalPS   = make_pixel_shader("deferred-debug-view-normal-ps", "PSNormalMain");
    m_MaterialPS = make_pixel_shader("deferred-debug-view-material-ps", "PSMaterialMain");
    m_EmissivePS = make_pixel_shader("deferred-debug-view-emissive-ps", "PSEmissiveMain");
    m_ObjectMaterialIdPS = make_pixel_shader("deferred-debug-view-object-material-id-ps", "PSObjectMaterialIdMain");

    m_AlbedoPipeline   = make_pipeline("deferred-debug-view-albedo", m_AlbedoPS);
    m_NormalPipeline   = make_pipeline("deferred-debug-view-normal", m_NormalPS);
    m_MaterialPipeline = make_pipeline("deferred-debug-view-material", m_MaterialPS);
    m_EmissivePipeline = make_pipeline("deferred-debug-view-emissive", m_EmissivePS);
    m_ObjectMaterialIdPipeline = make_pipeline("deferred-debug-view-object-material-id", m_ObjectMaterialIdPS);
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
        .size   = gfx::ConstantBufferElementSize(sizeof(BindlessInfo)),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    rg::RenderPassBuilder debug_pass_builder(render_graph);
    debug_pass_builder
        .SetName(std::format("DeferredDebugViewPass-{}-{}", view_name, render_graph.GetFrameIndex()))
        .SetRenderTarget(target, true)
        .Read(gbuffer.albedo, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.normal, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.material, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.emissive, {}, gfx::PipelineStage::PixelShader)
        .Read(gbuffer.object_material_id, {}, gfx::PipelineStage::PixelShader)
        .Read(bindless_info, 0, 1, sizeof(BindlessInfo), gfx::PipelineStage::PixelShader)
        .AddSampler(sampler)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();

            gfx::GPUBufferView::MappedSpan<BindlessInfo>(pass.Resolve(bindless_info)).front() = {
                .gbuffer_albedo   = pass.GetBindless(gbuffer.albedo),
                .gbuffer_normal   = pass.GetBindless(gbuffer.normal),
                .gbuffer_material = pass.GetBindless(gbuffer.material),
                .gbuffer_emissive = pass.GetBindless(gbuffer.emissive),
                .object_material_id = pass.GetBindless(gbuffer.object_material_id),
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

            cmd.SetPipeline(*selected_pipeline);
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info),
            });
            cmd.Draw(3);
        })
        .Finish();

    return target;
}

}  // namespace hitagi::render
