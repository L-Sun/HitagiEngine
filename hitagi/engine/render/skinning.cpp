module;
#include "interop/tracy_macros.hpp"

export module render:skinning;
import interop.tracy;
import std;
import asset;
import gfx;
import math;
import utils;

export namespace hitagi::render {

// Per-renderer pipeline and per-Build graph handles. Graph owns buffers through
// GPU completion and pools the transient output buffers between executions.
class GpuSkinning {
public:
    using Streams = utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute>;
    void BeginFrame() {
        m_Streams.clear();
        m_Worlds.clear();
        std::erase_if(m_Bindings, [](const auto& entry) { return entry.second.owner.expired(); });
    }
    auto Build(rg::RenderGraph& graph, const asset::ResourceLoadContext& context,
               const std::shared_ptr<asset::VertexArray>& vertices,
               const std::shared_ptr<const asset::SkinPose>& pose) -> const Streams&;
private:
    auto BuildPalette(rg::RenderGraph& graph, const asset::ResourceLoadContext& context,
                      const std::shared_ptr<const asset::SkinPose>& pose) -> rg::GPUBufferHandle;
    struct BindingBuffer {
        std::weak_ptr<const asset::Skin> owner;
        std::shared_ptr<gfx::GPUBuffer> buffer;
    };
    std::pmr::unordered_map<const asset::Skin*, BindingBuffer> m_Bindings;
    std::pmr::unordered_map<const void*, rg::GPUBufferHandle> m_Worlds;
    std::shared_ptr<gfx::ComputePipeline> m_PalettePipeline;
    std::shared_ptr<gfx::ComputePipeline> m_Pipeline;
    std::pmr::map<std::pair<const asset::VertexArray*, const asset::SkinPose*>, Streams> m_Streams;
};
}

namespace hitagi::render {
struct SkinArguments {
    std::array<gfx::BindlessHandle, 4> inputs, outputs;
    gfx::BindlessHandle indices, weights, palette;
    std::uint32_t count, attributes;
};

struct PaletteArguments {
    math::mat4f world_to_mesh;
    gfx::BindlessHandle worlds, inverse_bind, output;
    std::uint32_t count;
};

auto GpuSkinning::BuildPalette(rg::RenderGraph& graph, const asset::ResourceLoadContext& context,
                              const std::shared_ptr<const asset::SkinPose>& pose) -> rg::GPUBufferHandle {
    ZoneScopedNS("GpuSkin.BuildPalettePass", 0);
    const auto count = pose->joint_worlds->size();
    if (!pose->skin || !count || count != pose->skin->GetInverseBindMatrices().size() ||
        count > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("Invalid GPU palette binding");
    if (!m_PalettePipeline) {
        auto shader = gfx::Shader::Create(context.device, context.shader_compiler,
            {.name = "SkinPalette", .type = gfx::ShaderType::Compute, .entry = "main", .program = {.modules = {{.name = "skin_palette"}}}});
        if (!shader) throw std::runtime_error("Skin palette shader compilation failed");
        m_PalettePipeline = gfx::ComputePipeline::Create(context.device, context.bindings, {.name = "SkinPalette"}, shader);
    }
    auto& binding = m_Bindings[pose->skin.get()];
    if (binding.owner.expired()) {
        ZoneScopedNS("GpuSkin.UploadInverseBind", 0);
        binding.owner = pose->skin;
        binding.buffer = gfx::GPUBuffer::Create(context.device,
            {.name = "InverseBind", .size = count * sizeof(math::mat4f),
             .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::CopyDst},
            std::as_bytes(pose->skin->GetInverseBindMatrices()));
    }
    auto [world, inserted] = m_Worlds.try_emplace(pose->joint_worlds.get());
    if (inserted) {
        ZoneScopedNS("GpuSkin.UploadJointWorlds", 0);
        // Immutable per-frame upload shared by every mesh using this binding order.
        auto upload = gfx::GPUBuffer::Create(context.device,
            {.name = "JointWorlds", .size = count * sizeof(math::mat4f),
             .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite},
            std::as_bytes(std::span<const math::mat4f>(*pose->joint_worlds)));
        world->second = graph.Import(upload);
    }
    const auto output = graph.Create({.name = "ComputedSkinPalette", .size = count * sizeof(math::mat4f),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::StorageWrite});
    const auto arguments = graph.Create({.name = "PaletteArguments", .size = sizeof(PaletteArguments),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite});
    rg::ComputePassBuilder builder(graph);
    builder.SetName(std::format("GpuSkinPalette-{}", arguments.index));
    const auto worlds = builder.Read(world->second, {.element_size = sizeof(math::mat4f), .element_count = count});
    const auto inverse = builder.Read(graph.Import(binding.buffer), {.element_size = sizeof(math::mat4f), .element_count = count});
    const auto result = builder.Write(output, {.element_size = sizeof(math::mat4f), .element_count = count});
    const auto args = builder.Read(arguments, {.element_size = sizeof(PaletteArguments), .element_count = 1});
    builder.SetExecutor([pose, pipeline = m_PalettePipeline, worlds, inverse, result, args, count]
        (const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        PaletteArguments data{pose->world_to_mesh, pass.Resolve(worlds).GetBindlessHandle(),
            pass.Resolve(inverse).GetBindlessHandle(), pass.Resolve(result).GetBindlessHandle(), static_cast<std::uint32_t>(count)};
        { auto mapped = pass.Resolve(args).GetMappedSpan<PaletteArguments>(); mapped[0] = data; }
        auto& cmd = pass.GetCmd();
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({.handle = pass.Resolve(args).GetBindlessHandle()});
        cmd.Dispatch({static_cast<std::uint32_t>((count + 63) / 64), 1, 1});
    });
    builder.Finish();
    return output;
}

auto GpuSkinning::Build(rg::RenderGraph& graph, const asset::ResourceLoadContext& context,
                       const std::shared_ptr<asset::VertexArray>& vertices,
                       const std::shared_ptr<const asset::SkinPose>& pose) -> const Streams& {
    ZoneScopedNS("GpuSkin.BuildDeformationPass", 0);
    auto [it, inserted] = m_Streams.try_emplace({vertices.get(), pose.get()});
    if (!inserted) return it->second;
    auto& streams = it->second;
    if (!pose || vertices->Size() == 0) return streams;
    if ((!pose->joint_worlds && pose->matrices.empty()) || vertices->Size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("Invalid GPU skin pose");
    if (!m_Pipeline) {
        auto shader = gfx::Shader::Create(context.device, context.shader_compiler,
            {.name = "Skinning", .type = gfx::ShaderType::Compute, .entry = "main", .program = {.modules = {{.name = "skinning"}}}});
        if (!shader) throw std::runtime_error("Skinning shader compilation failed");
        m_Pipeline = gfx::ComputePipeline::Create(context.device, context.bindings, {.name = "Skinning"}, shader);
    }
    vertices->Load(context);
    const auto count = vertices->Size();
    const auto joint_count = pose->joint_worlds ? pose->joint_worlds->size() : pose->matrices.size();
    auto palette = pose->joint_worlds ? BuildPalette(graph, context, pose) :
        graph.Create({.name = "SkinPalette", .size = joint_count * sizeof(math::mat4f),
            .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite});
    auto arguments = graph.Create({.name = "SkinArguments", .size = sizeof(SkinArguments),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite});
    rg::ComputePassBuilder builder(graph);
    builder.SetName(std::format("GpuSkinning-{}", arguments.index));
    const auto palette_access = builder.Read(palette, {.element_size = sizeof(math::mat4f), .element_count = joint_count});
    const auto arguments_access = builder.Read(arguments, {.element_size = sizeof(SkinArguments), .element_count = 1});
    const auto read_attribute = [&](asset::VertexAttribute attribute, std::size_t stride) {
        const auto data = vertices->GetAttributeData(attribute);
        if (!data || !data->get().gpu_buffer) throw std::invalid_argument("Missing GPU skin attribute");
        return builder.Read(graph.Import(data->get().gpu_buffer), {.element_size = stride, .element_count = count});
    };
    const auto indices = read_attribute(asset::VertexAttribute::BlendIndex, 16);
    const auto weights = read_attribute(asset::VertexAttribute::BlendWeight, 16);
    std::array<rg::GPUBufferEdgeHandle, 4> inputs{}, outputs{};
    constexpr std::array attributes{asset::VertexAttribute::Position, asset::VertexAttribute::Normal,
        asset::VertexAttribute::Tangent, asset::VertexAttribute::Bitangent};
    std::uint32_t flags = 0;
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (!vertices->GetAttributeData(attributes[i])) continue;
        flags |= 1u << i;
        inputs[i] = read_attribute(attributes[i], sizeof(math::vec3f));
        streams[attributes[i]] = graph.Create({.name = "SkinnedStream", .size = count * sizeof(math::vec3f),
            .usages = gfx::GPUBufferUsageFlags::StorageWrite | gfx::GPUBufferUsageFlags::Vertex | gfx::GPUBufferUsageFlags::CopySrc});
        outputs[i] = builder.Write(streams[attributes[i]], {.element_size = sizeof(math::vec3f), .element_count = count});
    }
    builder.SetExecutor([pose, pipeline = m_Pipeline, palette_access, arguments_access, indices, weights, inputs, outputs, flags, count]
                        (const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        if (!pose->joint_worlds) {
            auto mapped = pass.Resolve(palette_access).GetMappedSpan<math::mat4f>();
            for (std::size_t i = 0; i < pose->matrices.size(); ++i) mapped[i] = pose->matrices[i];
        }
        SkinArguments data{.indices = pass.Resolve(indices).GetBindlessHandle(),
            .weights = pass.Resolve(weights).GetBindlessHandle(), .palette = pass.Resolve(palette_access).GetBindlessHandle(),
            .count = static_cast<std::uint32_t>(count), .attributes = flags};
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            if (!(flags & (1u << i))) continue;
            data.inputs[i] = pass.Resolve(inputs[i]).GetBindlessHandle();
            data.outputs[i] = pass.Resolve(outputs[i]).GetBindlessHandle();
        }
        { auto mapped = pass.Resolve(arguments_access).GetMappedSpan<SkinArguments>(); mapped[0] = data; }
        auto& cmd = pass.GetCmd();
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({.handle = pass.Resolve(arguments_access).GetBindlessHandle()});
        cmd.Dispatch({static_cast<std::uint32_t>((count + 63) / 64), 1, 1});
    });
    builder.Finish();
    return streams;
}
}
