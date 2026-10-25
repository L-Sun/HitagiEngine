#include "test_macros.hpp"
#ifdef __APPLE__
import std;
import interop.gtest;
import gfx;
import gfx.metal;
import math;
import utils;

using namespace hitagi;
using namespace hitagi::gfx;
using namespace hitagi::utils;

TEST(MetalQueueTest, ComputeTextureToGraphicsToCopy) {
    auto           device = create_device(Device::Type::Metal, "TextureQueueChain");
    CommandQueues  queues(*device);
    auto           bindings = BindlessUtils::Create(*device);
    ShaderCompiler compiler{"TextureQueueChain"};
    auto           texture = Texture::Create(
        *device, queues, *bindings,
        {
            .width  = 1,
            .height = 1,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::SRV | TextureUsageFlags::UAV,
        });
    auto             storage = TextureView::Create(*device, *bindings, {.texture = texture, .type = TextureViewType::ShaderWrite});
    auto             sampled = TextureView::Create(*device, *bindings, {.texture = texture, .type = TextureViewType::ShaderRead});
    auto             sampler = Sampler::Create(*device, *bindings, {});
    const std::array handles{storage->GetBindlessHandle(), sampled->GetBindlessHandle(), sampler->GetBindlessHandle()};
    auto             arguments = GPUBuffer::Create(
        *device,
        {
            .size   = sizeof(handles),
            .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
        },
        std::as_bytes(std::span(handles)));
    auto                   argument_view     = GPUBufferView::Create(*device, *bindings, {.buffer = arguments, .element_size = sizeof(handles)});
    const std::pmr::string source            = R"(
        import bindless;
        struct Arguments { hitagi::BindlessHandle storage; hitagi::Texture sampled; hitagi::Sampler sampler; };
        [numthreads(1, 1, 1)]
        void cs_main() {
            Arguments args = hitagi::load_bindless<Arguments>();
            DescriptorHandle<RWTexture2D<float4>> output = DescriptorHandle<RWTexture2D<float4>>(uint2(args.storage.index, 0));
            nonuniform(output)[uint2(0, 0)] = float4(1, 0, 0, 1);
        }
        float4 vs_main(uint id : SV_VertexID) : SV_Position {
            float2 p = float2((id << 1) & 2, id & 2);
            return float4(p * 2 - 1, 0, 1);
        }
        float4 ps_main() : SV_Target {
            Arguments args = hitagi::load_bindless<Arguments>();
            return args.sampled.sample_level<float4>(args.sampler.load(), float2(0.5), 0);
        }
    )";
    auto                   cs                = Shader::Create(*device, compiler, {.type = ShaderType::Compute, .entry = "cs_main", .program = {.modules = {{.source_code = source}}}});
    auto                   vs                = Shader::Create(*device, compiler, {.type = ShaderType::Vertex, .entry = "vs_main", .program = {.modules = {{.source_code = source}}}});
    auto                   ps                = Shader::Create(*device, compiler, {.type = ShaderType::Pixel, .entry = "ps_main", .program = {.modules = {{.source_code = source}}}});
    auto                   compute_pipeline  = ComputePipeline::Create(*device, *bindings, {}, cs);
    auto                   graphics_pipeline = RenderPipeline::Create(*device, *bindings, {}, {vs, ps});
    auto                   target            = Texture::Create(
        *device, queues, *bindings,
        {
            .width  = 4,
            .height = 4,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
        });
    auto target_view = TextureView::Create(*device, *bindings, {.texture = target, .type = TextureViewType::RenderTarget});
    auto readback    = GPUBuffer::Create(*device, {.size = 64, .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
    auto compute     = ComputeCommandContext::Create(*device, queues, *bindings);
    compute->Begin();
    compute->ResourceBarrier({}, {}, std::array{texture->Transition(BarrierAccess::ShaderWrite, TextureLayout::ShaderWrite, PipelineStage::ComputeShader)});
    compute->SetPipeline(*compute_pipeline);
    compute->PushBindlessMetaInfo({.handle = argument_view->GetBindlessHandle()});
    static_cast<MetalComputeCommandBuffer&>(*compute).GetMetalEncoder()->dispatchThreads({1, 1, 1}, {1, 1, 1});
    compute->End();
    auto graphics = GraphicsCommandContext::Create(*device, queues, *bindings);
    graphics->Begin();
    graphics->ResourceBarrier(
        {}, {}, std::array{
                    texture->Transition(BarrierAccess::ShaderRead, TextureLayout::ShaderRead, PipelineStage::PixelShader),
                    target->Transition(BarrierAccess::RenderTarget, TextureLayout::RenderTarget, PipelineStage::All),
                });
    graphics->BeginRendering(*target_view);
    graphics->SetPipeline(*graphics_pipeline);
    graphics->SetViewPort({.width = 4, .height = 4});
    graphics->SetScissorRect({.width = 4, .height = 4});
    graphics->PushBindlessMetaInfo({.handle = argument_view->GetBindlessHandle()});
    graphics->Draw(3);
    graphics->EndRendering();
    graphics->End();
    auto copy = CopyCommandContext::Create(*device, queues);
    copy->Begin();
    copy->ResourceBarrier({}, std::array{readback->Transition(BarrierAccess::CopyDst, PipelineStage::Copy)},
                          std::array{target->Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy)});
    copy->CopyTextureToBuffer(*target, {}, {4, 4, 1}, *readback, 0);
    copy->End();
    auto compute_done  = Fence::Create(*device);
    auto graphics_done = Fence::Create(*device);
    queues.Get(CommandType::Compute).Submit({{*compute}}, {}, {{FenceSignalInfo{.fence = *compute_done, .value = 1}}});
    queues.Get(CommandType::Graphics).Submit({{*graphics}}, {{FenceWaitInfo{.fence = *compute_done, .value = 1}}}, {{FenceSignalInfo{.fence = *graphics_done, .value = 1}}});
    queues.Get(CommandType::Copy).Submit({{*copy}}, {{FenceWaitInfo{.fence = *graphics_done, .value = 1}}});
    queues.WaitIdle();
    const auto* pixels = readback->Map();
    for (std::size_t pixel = 0; pixel < 16; ++pixel) {
        EXPECT_EQ(pixels[pixel * 4], std::byte{255});
        EXPECT_EQ(pixels[pixel * 4 + 1], std::byte{0});
        EXPECT_EQ(pixels[pixel * 4 + 2], std::byte{0});
        EXPECT_EQ(pixels[pixel * 4 + 3], std::byte{255});
    }
    readback->UnMap();
}

TEST(MetalQueueTest, SubmissionIsAsyncAndOwnsRerecordedResources) {
    auto                                                      device = create_device(Device::Type::Metal);
    CommandQueues                                             queues(*device);
    auto&                                                     queue   = queues.Get(CommandType::Copy);
    auto                                                      gate    = Fence::Create(*device);
    auto                                                      done    = Fence::Create(*device);
    auto                                                      context = CopyCommandContext::Create(*device, queues);
    std::array<std::shared_ptr<GPUBuffer>, 2>                 outputs;
    std::array<std::weak_ptr<const MetalCommandRecording>, 2> recordings;
    // Failure protection: a regressed CPU-waiting Submit must not hang the test.
    std::mutex              mutex;
    std::condition_variable ready;
    bool                    submitted = false;
    std::jthread            watchdog([&] {
        std::unique_lock lock(mutex);
        ready.wait_for(lock, std::chrono::seconds(2), [&] { return submitted; });
        gate->Signal(1);
    });
    struct FrameEnd {
        Device& device;
        ~FrameEnd() { device.EndFrame(); }
    };
    for (unsigned round = 0; round < 2; ++round) {
        device->BeginFrame();
        FrameEnd frame_end{*device};
        const std::array data{std::uint32_t(0x11223344 + round)};
        auto             input = GPUBuffer::Create(*device, {.size = sizeof(data), .usages = GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::MapWrite}, std::as_bytes(std::span(data)));
        outputs[round]         = GPUBuffer::Create(*device, {.size = sizeof(data), .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
        context->Begin();
        context->CopyBuffer(*input, 0, *outputs[round], 0, sizeof(data));
        context->End();
        recordings[round] = static_cast<MetalTransferCommandBuffer&>(*context).GetRecording();
        queue.Submit({{*context}}, {{FenceWaitInfo{*gate, 1}}}, {{FenceSignalInfo{*done, round + 1}}});
        // Drain temporary native references while the GPU is still gated. The
        // queue must retain submitted recordings across CPU frame boundaries.
        device->EndFrame();
        EXPECT_FALSE(done->Wait(round + 1, std::chrono::milliseconds(0)));
    }
    context.reset();
    for (const auto& recording : recordings) EXPECT_FALSE(recording.expired());
    {
        std::scoped_lock lock(mutex);
        submitted = true;
    }
    ready.notify_one();
    ASSERT_TRUE(done->Wait(2, std::chrono::seconds(5)));
    queue.WaitIdle();
    for (const auto& recording : recordings) EXPECT_TRUE(recording.expired());
    for (unsigned round = 0; round < 2; ++round) {
        std::uint32_t value;
        std::memcpy(&value, outputs[round]->Map(), sizeof(value));
        outputs[round]->UnMap();
        EXPECT_EQ(value, 0x11223344u + round);
    }
}

TEST(MetalQueueTest, BindlessSnapshotsReuseUnchangedTablesAndRetainRecycledSlots) {
    auto       device           = create_device(Device::Type::Metal);
    auto       bindings         = BindlessUtils::Create(*device);
    auto&      backend          = static_cast<MetalBindlessUtils&>(*bindings);
    auto       buffer           = GPUBuffer::Create(*device, {.size = 16, .usages = GPUBufferUsageFlags::StorageRead});
    auto       view             = GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .element_size = 16});
    const auto handle           = view->GetBindlessHandle();
    const auto original_address = static_cast<MetalBuffer&>(*buffer).native->gpuAddress();
    auto       first            = backend.GetSnapshot();
    EXPECT_EQ(first, backend.GetSnapshot());
    view.reset();
    buffer.reset();
    auto replacement      = GPUBuffer::Create(*device, {.size = 16, .usages = GPUBufferUsageFlags::StorageRead});
    auto replacement_view = GPUBufferView::Create(*device, *bindings, {.buffer = replacement, .element_size = 16});
    EXPECT_EQ(replacement_view->GetBindlessHandle().index, handle.index);
    auto second = backend.GetSnapshot();
    EXPECT_NE(first, second);
    const auto* old_table = static_cast<const std::uint64_t*>(first->buffer->contents());
    const auto* new_table = static_cast<const std::uint64_t*>(second->buffer->contents());
    EXPECT_EQ(old_table[handle.index], original_address);
    EXPECT_EQ(new_table[handle.index], static_cast<MetalBuffer&>(*replacement).native->gpuAddress());
    EXPECT_NE(old_table[handle.index], new_table[handle.index]);
}

TEST(MetalQueueTest, RejectsForeignFencesBeforeEnqueuingWaits) {
    auto          first  = create_device(Device::Type::Metal);
    auto          second = create_device(Device::Type::Metal);
    CommandQueues queues(*first);
    auto          foreign = Fence::Create(*second);
    EXPECT_THROW(queues.Get(CommandType::Graphics).Submit({}, {{FenceWaitInfo{*foreign, 1}}}), std::runtime_error);
    auto done = Fence::Create(*first);
    queues.Get(CommandType::Graphics).Submit({}, {}, {{FenceSignalInfo{*done, 1}}});
    EXPECT_TRUE(done->Wait(1, std::chrono::seconds(5)));
}
TEST(MetalQueueTest, MetadataUploadSpansChunksWithoutOverwritingEarlierDispatches) {
    auto               device = create_device(Device::Type::Metal);
    CommandQueues      queues(*device);
    auto               bindings = BindlessUtils::Create(*device);
    ShaderCompiler     compiler{"MetadataChunks"};
    constexpr unsigned count       = 300;
    auto               output      = GPUBuffer::Create(*device, {.size = count * 4, .usages = GPUBufferUsageFlags::StorageWrite | GPUBufferUsageFlags::CopySrc});
    auto               output_view = GPUBufferView::Create(*device, *bindings, {.buffer = output, .type = GPUBufferViewType::StorageWrite, .element_size = 4, .element_count = count});
    struct Arguments {
        BindlessHandle output;
        std::uint32_t  index;
        std::uint32_t  value;
    };
    std::array<Arguments, count> records;
    for (unsigned i = 0; i < count; ++i) records[i] = {output_view->GetBindlessHandle(), i, i * 13 + 7};
    auto input      = GPUBuffer::Create(*device, {.size = sizeof(records), .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite}, std::as_bytes(std::span(records)));
    auto input_view = GPUBufferView::Create(*device, *bindings, {.buffer = input, .element_size = sizeof(Arguments), .element_count = count});
    auto shader     = Shader::Create(*device, compiler, {.type = ShaderType::Compute, .entry = "main", .program = {.modules = {{.source_code = R"(
        import bindless;
        struct Arguments { hitagi::RWSimpleBuffer output; uint index; uint value; };
        [numthreads(1, 1, 1)]
        void main() {
            Arguments args = hitagi::load_bindless<Arguments>();
            args.output.store<uint>(args.value, args.index * 4);
        }
    )"}}}});
    auto pipeline   = ComputePipeline::Create(*device, *bindings, {}, shader);
    auto context    = ComputeCommandContext::Create(*device, queues, *bindings);
    context->Begin();
    context->SetPipeline(*pipeline);
    for (unsigned i = 0; i < count; ++i) {
        context->PushBindlessMetaInfo({.handle = input_view->GetBindlessHandle(), .record_index = i, .record_stride = sizeof(Arguments)});
        static_cast<MetalComputeCommandBuffer&>(*context).GetMetalEncoder()->dispatchThreads({1, 1, 1}, {1, 1, 1});
    }
    context->End();
    auto done = Fence::Create(*device);
    queues.Get(CommandType::Compute).Submit({{*context}}, {}, {{FenceSignalInfo{*done, 1}}});
    context.reset();
    pipeline.reset();
    shader.reset();
    input_view.reset();
    input.reset();
    output_view.reset();
    auto readback = GPUBuffer::Create(*device, {.size = count * 4, .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
    auto copy     = CopyCommandContext::Create(*device, queues);
    copy->Begin();
    copy->CopyBuffer(*output, 0, *readback, 0, count * 4);
    copy->End();
    queues.Get(CommandType::Copy).Submit({{*copy}}, {{FenceWaitInfo{*done, 1}}});
    queues.Get(CommandType::Copy).WaitIdle();
    const auto* values = reinterpret_cast<const std::uint32_t*>(readback->Map());
    for (unsigned i = 0; i < count; ++i) EXPECT_EQ(values[i], i * 13 + 7);
    readback->UnMap();
}

TEST(MetalQueueTest, ConcurrentSubmissionsSignalIndependentTimelines) {
    auto                                  device = create_device(Device::Type::Metal);
    CommandQueues                         queues(*device);
    std::array<std::shared_ptr<Fence>, 4> fences;
    for (auto& fence : fences) fence = Fence::Create(*device);
    std::array<std::exception_ptr, 4> errors;
    std::pmr::vector<std::jthread>    threads;
    for (unsigned i = 0; i < fences.size(); ++i) threads.emplace_back([&, i] {
        try {
            for (unsigned value = 1; value <= 64; ++value)
                queues.Get(CommandType::Compute).Submit({}, {}, {{FenceSignalInfo{*fences[i], value}}});
        } catch (...) {
            errors[i] = std::current_exception();
        }
    });
    threads.clear();
    for (const auto& error : errors) EXPECT_FALSE(error);
    for (auto& fence : fences) EXPECT_TRUE(fence->Wait(64, std::chrono::seconds(5)));
}

#endif
