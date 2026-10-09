export module render:deferred_lighting;
import std;
import utils;
import math;
import core;
import gfx;
import asset;

import :types;
import :gbuffer;

export namespace hitagi::render::passes {

class DeferredLighting {
public:
    DeferredLighting(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, ShaderSource shader);

    struct BindlessInfo {
        gfx::BindlessHandle frame_constant;
        gfx::BindlessHandle gbuffer_albedo;
        gfx::BindlessHandle gbuffer_normal;
        gfx::BindlessHandle gbuffer_material;
        gfx::BindlessHandle gbuffer_emissive;
        gfx::BindlessHandle sampler;
    };

    auto GetPipeline(gfx::Format target_format) -> std::shared_ptr<gfx::RenderPipeline>;

    auto Build(
        RenderContext&                              context,
        const GBufferOutput&                        gbuffer,
        rg::GPUBufferHandle                         frame_constant,
        rg::GPUBufferHandle                         bindless_info,
        rg::SamplerHandle                           sampler,
        const std::shared_ptr<gfx::RenderPipeline>& pipeline,
        rg::TextureHandle                           target) -> rg::TextureHandle;

private:
    void EnsureResources(gfx::Format target_format);

    gfx::Device&                         m_Device;
    gfx::BindlessUtils&                  m_Bindings;
    const gfx::ShaderCompiler&           m_ShaderCompiler;
    ShaderSource                         m_Shader;
    std::shared_ptr<gfx::Shader>         m_VS;
    std::shared_ptr<gfx::Shader>         m_PS;
    std::shared_ptr<gfx::RenderPipeline> m_Pipeline;
    gfx::Format                          m_TargetFormat = gfx::Format::UNKNOWN;
};

}  // namespace hitagi::render::passes

namespace hitagi::render {

passes::DeferredLighting::DeferredLighting(gfx::Device& device, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, ShaderSource shader)
    : m_Device(device), m_Bindings(bindings), m_ShaderCompiler(compiler), m_Shader(std::move(shader)) {}

void passes::DeferredLighting::EnsureResources(gfx::Format target_format) {
    if (m_Pipeline != nullptr && m_TargetFormat == target_format) return;

    if (m_VS == nullptr) {
        m_VS = hitagi::gfx::Shader::Create(m_Device, m_ShaderCompiler, {
                                                                           .name        = "deferred-lighting-vs",
                                                                           .type        = gfx::ShaderType::Vertex,
                                                                           .entry       = "VSMain",
                                                                           .source_code = m_Shader.code,
                                                                           .path        = m_Shader.path,
                                                                       });
        m_PS = hitagi::gfx::Shader::Create(m_Device, m_ShaderCompiler, {
                                                                           .name        = "deferred-lighting-ps",
                                                                           .type        = gfx::ShaderType::Pixel,
                                                                           .entry       = "PSMain",
                                                                           .source_code = m_Shader.code,
                                                                           .path        = m_Shader.path,
                                                                       });
    }

    m_Pipeline     = hitagi::gfx::RenderPipeline::Create(m_Device, m_Bindings,
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
    RenderContext&                       context,
    const GBufferOutput&                 gbuffer,
    rg::GPUBufferHandle                  frame_constant,
    rg::GPUBufferHandle                  bindless_info,
    rg::SamplerHandle                    sampler,
    const std::shared_ptr<gfx::RenderPipeline>& pipeline,
    rg::TextureHandle                    target) -> rg::TextureHandle {
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
    lighting_pass_builder.SetName(std::format("DeferredLightingPass-{}", render_graph.GetFrameIndex()));
    lighting_pass_builder.SetRenderTarget(target, true);
    const auto albedo_access         = lighting_pass_builder.Read(gbuffer.albedo, {}, gfx::PipelineStage::PixelShader);
    const auto normal_access         = lighting_pass_builder.Read(gbuffer.normal, {}, gfx::PipelineStage::PixelShader);
    const auto material_access       = lighting_pass_builder.Read(gbuffer.material, {}, gfx::PipelineStage::PixelShader);
    const auto emissive_access       = lighting_pass_builder.Read(gbuffer.emissive, {}, gfx::PipelineStage::PixelShader);
    const auto frame_constant_access = lighting_pass_builder.Read(frame_constant, {.offset = 0, .element_size = sizeof(FrameConstant), .element_count = 1}, gfx::PipelineStage::PixelShader);
    const auto bindless_info_access  = lighting_pass_builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(BindlessInfo), .element_count = 1}, gfx::PipelineStage::PixelShader);
    lighting_pass_builder.AddSampler(sampler);
    lighting_pass_builder.SetExecutor([=](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        pass.Resolve(bindless_info_access).GetMappedSpan<BindlessInfo>().front() = {
            .frame_constant   = pass.Resolve(frame_constant_access).GetBindlessHandle(),
            .gbuffer_albedo   = pass.Resolve(albedo_access).GetBindlessHandle(),
            .gbuffer_normal   = pass.Resolve(normal_access).GetBindlessHandle(),
            .gbuffer_material = pass.Resolve(material_access).GetBindlessHandle(),
            .gbuffer_emissive = pass.Resolve(emissive_access).GetBindlessHandle(),
            .sampler          = pass.Resolve(sampler).GetBindlessHandle(),
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
            .handle = pass.Resolve(bindless_info_access).GetBindlessHandle(),
        });
        cmd.Draw(3);
    });
    lighting_pass_builder.Finish();

    return target;
}

}  // namespace hitagi::render
