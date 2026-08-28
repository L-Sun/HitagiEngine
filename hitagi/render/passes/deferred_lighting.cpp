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

    m_Pipeline = m_Device.CreateRenderPipeline(
        {
            .name                = "deferred-lighting",
            .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
            .rasterization_state = {.cull_mode = gfx::CullMode::None},
            .render_format       = target_format,
        },
        {m_VS, m_PS});
    m_TargetFormat = target_format;
}

auto passes::DeferredLighting::GetPipeline(gfx::Format target_format) -> std::shared_ptr<gfx::RenderPipeline> {
    EnsureResources(target_format);
    return m_Pipeline;
}

auto passes::DeferredLighting::Build(
    RenderContext&           context,
    const GBufferOutput&     gbuffer,
    rg::GPUBufferHandle      frame_constant,
    rg::GPUBufferHandle      bindless_info,
    rg::SamplerHandle        sampler,
    std::shared_ptr<gfx::RenderPipeline> pipeline,
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
        !pipeline) {
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
        .Read(frame_constant, 0, 1, sizeof(FrameConstant), gfx::PipelineStage::PixelShader)
        .Read(bindless_info, 0, 1, sizeof(BindlessInfo), gfx::PipelineStage::PixelShader)
        .AddSampler(sampler)
        .SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
            auto& cmd = pass.GetCmd();

            gfx::GPUBufferView::MappedSpan<BindlessInfo>(pass.Resolve(bindless_info)).front() = {
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

            cmd.SetPipeline(*pipeline);
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_info),
            });
            cmd.Draw(3);
        })
        .Finish();

    return target;
}

}  // namespace hitagi::render
