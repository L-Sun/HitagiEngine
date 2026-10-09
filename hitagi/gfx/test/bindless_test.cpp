#include "test_macros.hpp"
import interop.gtest;
#ifdef _WIN32
import interop.dx12;
#endif

import std;
import gfx;
import gfx.vulkan;
#ifdef _WIN32
import gfx.dx12;
#endif
import asset;
import math;
import utils;

using namespace hitagi;
using namespace hitagi::gfx;
using namespace hitagi::utils;

TEST(BindlessVulkanTest, SharedHeapsAcrossCommandBuffers) {
    auto                                                      device = create_device(Device::Type::Vulkan, "SharedHeaps");
    hitagi::gfx::CommandQueues                                queues(*device);
    auto                                                      bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler                               compiler{"Tests"};
    std::vector<std::shared_ptr<CommandContext>>              contexts;
    std::vector<std::reference_wrapper<const CommandContext>> graphics, compute;
    for (std::size_t index = 0; index < 300; ++index) {
        auto context = hitagi::gfx::CommandContext::Create(*device, queues, *bindings, index % 2 ? CommandType::Graphics : CommandType::Compute);
        context->Begin();
        context->End();
        (index % 2 ? graphics : compute).emplace_back(*context);
        contexts.emplace_back(std::move(context));
    }
    queues.Get(CommandType::Graphics).Submit(graphics);
    queues.Get(CommandType::Compute).Submit(compute);
    queues.WaitIdle();
}

class BindlessTest : public testing::TestWithParam<Device::Type> {};

TEST_P(BindlessTest, RejectsForeignCreationDependencies) {
    auto          device       = create_device(GetParam(), "DependencyOwner");
    auto          other_device = create_device(GetParam(), "ForeignDependencyOwner");
    CommandQueues queues(*device);
    CommandQueues other_queues(*other_device);
    auto          bindings       = BindlessUtils::Create(*device);
    auto          other_bindings = BindlessUtils::Create(*other_device);
    auto          buffer         = GPUBuffer::Create(*device, {.size = 256, .usages = GPUBufferUsageFlags::StorageRead});
    auto          other_buffer   = GPUBuffer::Create(*other_device, {.size = 256, .usages = GPUBufferUsageFlags::StorageRead});

    EXPECT_THROW(GPUBufferView::Create(*device, *other_bindings, {.buffer = buffer}), std::invalid_argument);
    EXPECT_THROW(GPUBufferView::Create(*device, *bindings, {.buffer = other_buffer}), std::invalid_argument);
    EXPECT_THROW(Sampler::Create(*device, *other_bindings, {}), std::invalid_argument);
    EXPECT_THROW(GraphicsCommandContext::Create(*device, other_queues, *bindings), std::invalid_argument);
    EXPECT_THROW(CopyCommandContext::Create(*device, other_queues), std::invalid_argument);
    EXPECT_THROW(CommandContext::Create(*device, queues, *bindings, static_cast<CommandType>(255)), std::invalid_argument);
    EXPECT_THROW(CommandQueue::Create(*device, static_cast<CommandType>(255)), std::invalid_argument);
    EXPECT_THROW(SwapChain::Create(*device, queues.Get(CommandType::Copy), {}), std::invalid_argument);
    EXPECT_NO_THROW(GPUBufferView::Create(*device, *bindings, {.buffer = buffer}));
}
#ifdef _WIN32
INSTANTIATE_TEST_SUITE_P(Backends, BindlessTest, testing::Values(Device::Type::DX12, Device::Type::Vulkan));
#else
INSTANTIATE_TEST_SUITE_P(Backends, BindlessTest, testing::Values(Device::Type::Vulkan));
#endif

TEST_P(BindlessTest, SamplerHeapExhaustionAndReuse) {
    auto                                  device = create_device(GetParam(), "SamplerHeapExhaustion");
    hitagi::gfx::CommandQueues            queues(*device);
    auto                                  bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler           compiler{"Tests"};
    std::vector<std::shared_ptr<Sampler>> samplers;
    for (std::size_t index = 0; index < 128; ++index) {
        auto sampler = hitagi::gfx::Sampler::Create(*device, *bindings, {});
        ASSERT_TRUE(sampler->GetBindlessHandle());
        samplers.emplace_back(std::move(sampler));
    }
    EXPECT_THROW(hitagi::gfx::Sampler::Create(*device, *bindings, {}), std::runtime_error);
    const auto released = samplers.back()->GetBindlessHandle();
    samplers.pop_back();
    samplers.emplace_back(hitagi::gfx::Sampler::Create(*device, *bindings, {}));
    EXPECT_EQ(samplers.back()->GetBindlessHandle().index, released.index);
    EXPECT_EQ(samplers.back()->GetBindlessHandle().version, released.version + 1);
}

TEST_P(BindlessTest, RenderGraphResolvesSharedSamplerAcrossPassesAndFrames) {
    auto                         device   = create_device(GetParam(), "GraphSharedSampler");
    hitagi::gfx::CommandQueues   queues(*device);
    auto                         bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler  compiler{"Tests"};
    auto                         sampler  = hitagi::gfx::Sampler::Create(*device, *bindings, {});
    const auto                   bindless = sampler->GetBindlessHandle();
    const std::weak_ptr<Sampler> lifetime = sampler;
    {
        rg::RenderGraph         graph(*device, queues, *bindings);
        const rg::SamplerHandle handle     = graph.Import(sampler);
        const auto              undeclared = graph.Create(SamplerDesc{});
        sampler.reset();
        for (int frame = 0; frame < 2; ++frame) {
            int executed = 0;
            for (int index = 0; index < 3; ++index) {
                rg::ComputePassBuilder builder(graph);
                builder.AllowPassCulling(false);
                builder.AddSampler(handle);
                builder.AddSampler(handle);
                builder.SetExecutor([&](const rg::RenderGraph&, const rg::ComputePassNode& pass) {
                    ++executed;
                    const Sampler& resolved = pass.Resolve(handle);
                    EXPECT_EQ(&resolved, lifetime.lock().get());
                    EXPECT_EQ(resolved.GetBindlessHandle().index, bindless.index);
                    EXPECT_EQ(resolved.GetBindlessHandle().version, bindless.version);
                    EXPECT_THROW(pass.Resolve(undeclared), std::out_of_range);
                });
                ASSERT_TRUE(graph.IsValid(builder.Finish()));
            }
            ASSERT_TRUE(graph.Compile());
            graph.Execute();
            EXPECT_EQ(executed, 3);
            EXPECT_FALSE(lifetime.expired());
        }
    }
    EXPECT_TRUE(lifetime.expired());
    // Allocation order differs between backends; exhaust the pool to observe reuse.
    std::vector<std::shared_ptr<Sampler>> replacements;
    bool                                  reused = false;
    for (std::size_t index = 0; index < 128; ++index) {
        replacements.emplace_back(hitagi::gfx::Sampler::Create(*device, *bindings, {}));
        const auto replacement = replacements.back()->GetBindlessHandle();
        if (replacement.index == bindless.index) {
            reused = true;
            EXPECT_EQ(replacement.version, bindless.version + 1);
        }
    }
    EXPECT_TRUE(reused);
}

TEST_P(BindlessTest, RawBufferRoundTrip) {
    auto                                          device = create_device(GetParam(), "RawBufferRoundTrip");
    hitagi::gfx::CommandQueues                    queues(*device);
    auto                                          bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler                   compiler{"Tests"};
    const math::mat4f                             matrix{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};
    const BindlessHandle                          marker{.index = 123, .type = BindlessHandleType::Texture, .version = 7};
    const std::array<asset::MaterialDataValue, 6> values{1.0f, math::vec3f{2, 3, 4}, math::vec2f{5, 6}, math::vec4f{7, 8, 9, 10}, matrix, marker};
    const auto                                    encoded = asset::EncodeMaterialData(values);
    constexpr std::uint64_t                       offset  = 256;
    const auto                                    size    = encoded.GetDataSize();
    auto                                          input   = hitagi::gfx::GPUBuffer::Create(*device, {.size = offset + size, .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite});
    std::memcpy(input->Map() + offset, encoded.GetData(), size);
    input->UnMap();
    auto output      = hitagi::gfx::GPUBuffer::Create(*device, {.size = offset + size, .usages = GPUBufferUsageFlags::StorageWrite | GPUBufferUsageFlags::CopySrc});
    auto input_view  = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = input, .type = GPUBufferViewType::StorageRead, .offset = offset, .element_size = size});
    auto output_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = output, .type = GPUBufferViewType::StorageWrite, .offset = offset, .element_size = size});
    ASSERT_TRUE(input_view->GetBindlessHandle());
    ASSERT_TRUE(output_view->GetBindlessHandle());
    const std::array handles{input_view->GetBindlessHandle(), output_view->GetBindlessHandle()};
    auto             arguments     = hitagi::gfx::GPUBuffer::Create(*device, {.size = sizeof(handles), .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite}, std::as_bytes(std::span(handles)));
    auto             argument_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = arguments, .element_size = sizeof(handles)});

    const auto shader = hitagi::gfx::Shader::Create(*device, compiler, {.type = ShaderType::Compute, .entry = "main", .source_code = R"(
        #include "bindless.hlsl"
        struct Arguments { hitagi::SimpleBuffer input; hitagi::RWSimpleBuffer output; };
        struct Data {
            float scalar; float3 vector; float2 uv; float4 color;
            row_major float4x4 matrix;
            hitagi::BindlessHandle marker;
        };
        [numthreads(1, 1, 1)]
        void main() {
            Arguments arguments = hitagi::load_bindless<Arguments>();
            Data value = arguments.input.load<Data>();
            value.scalar += value.matrix[2][1];
            arguments.output.store<Data>(value);
        }
    )"});
    ASSERT_TRUE(shader);
    const auto pipeline = hitagi::gfx::ComputePipeline::Create(*device, *bindings, {}, shader);
    auto       context  = hitagi::gfx::ComputeCommandContext::Create(*device, queues, *bindings);
    context->Begin();
    context->ResourceBarrier({}, std::array{output->Transition(BarrierAccess::ShaderWrite, PipelineStage::ComputeShader)});
    context->SetPipeline(*pipeline);
    context->PushBindlessMetaInfo({.handle = argument_view->GetBindlessHandle()});
    // Dispatch through the backend: the public compute context has no Dispatch API yet.
    if (GetParam() == Device::Type::Vulkan) {
        static_cast<VulkanComputeCommandBuffer&>(*context).command_buffer.dispatch(1, 1, 1);
    }
#ifdef _WIN32
    else {
        static_cast<DX12ComputeCommandList&>(*context).command_list->Dispatch(1, 1, 1);
    }
#endif
    context->ResourceBarrier({}, std::array{output->Transition(BarrierAccess::CopySrc, PipelineStage::Copy)});
    context->End();
    auto& queue = queues.Get(CommandType::Compute);
    queue.Submit({{*context}});
    queue.WaitIdle();

    auto readback = hitagi::gfx::GPUBuffer::Create(*device, {.size = size, .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
    auto copy     = hitagi::gfx::CopyCommandContext::Create(*device, queues);
    copy->Begin();
    copy->ResourceBarrier({}, std::array{readback->Transition(BarrierAccess::CopyDst, PipelineStage::Copy)});
    copy->CopyBuffer(*output, offset, *readback, 0, size);
    copy->End();
    auto& copy_queue = queues.Get(CommandType::Copy);
    copy_queue.Submit({{*copy}});
    copy_queue.WaitIdle();
    const auto mapped = readback->Map();
    float      scalar;
    std::memcpy(&scalar, mapped, sizeof(scalar));
    EXPECT_FLOAT_EQ(scalar, 11.0f);
    EXPECT_EQ(std::memcmp(mapped + 4, encoded.GetData() + 4, size - 4), 0);
    readback->UnMap();
}

TEST_P(BindlessTest, StorageViewPreservesDenseElements) {
    auto       device       = create_device(GetParam(), "DenseStorageElements");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    const auto requirements = GPUBuffer::GetStorageViewRequirements(*device);
    EXPECT_GT(requirements.offset_alignment, 0);
    EXPECT_EQ(requirements.size_alignment, 4);
    const std::array<float, 3> input{1.0f, 2.0f, 3.0f};
    auto                       buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                                    .size   = sizeof(input),
                                                                                    .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                                },
                                                                       std::as_bytes(std::span(input)));
    auto                       view   = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                                                   .buffer        = buffer,
                                                                                                   .element_size  = sizeof(float),
                                                                                                   .element_count = input.size(),
                                                                                               });
    EXPECT_EQ(view->GetDesc().element_stride, sizeof(float));
    EXPECT_EQ(view->Size(), sizeof(input));
    auto mapped = view->GetMappedSpan<float>();
    EXPECT_EQ(mapped.size(), input.size());
    EXPECT_EQ(mapped.data()[1], 2.0f);
    EXPECT_EQ(mapped[2], 3.0f);
    if (requirements.offset_alignment > sizeof(float)) {
        EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .offset = sizeof(float), .element_size = sizeof(float)}), std::invalid_argument);
    }
}

TEST_P(BindlessTest, ExplicitStrideAndOverlappingViews) {
    auto       device       = create_device(GetParam(), "ExplicitStorageStride");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    const auto requirements = GPUBuffer::GetStorageViewRequirements(*device);
    const auto stride       = 2 * requirements.offset_alignment;
    auto                        buffer       = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                                           .size   = 2 * stride + sizeof(float),
                                                                                           .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                                       });
    auto                        view         = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                                                          .buffer         = buffer,
                                                                                                          .element_size   = sizeof(float),
                                                                                                          .element_count  = 0,
                                                                                                          .element_stride = stride,
                                                                                                      });
    EXPECT_EQ(view->GetDesc().element_count, 3);
    EXPECT_EQ(view->Size(), buffer->Size());
    {
        auto                       mapped = view->GetMappedSpan<float>();
        const std::array<float, 3> values{3.0f, 5.0f, 7.0f};
        std::ranges::copy(values, mapped.begin());
        EXPECT_THROW(mapped.data(), std::logic_error);
        EXPECT_EQ(*(1 + mapped.begin()), 5.0f);
        EXPECT_EQ(mapped.end() - mapped.begin(), 3);
        EXPECT_EQ(mapped.begin() - mapped.end(), -3);
    }
    auto tail = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .offset = stride, .element_size = sizeof(float), .element_count = 2, .element_stride = stride});
    auto last = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .offset = 2 * stride, .element_size = sizeof(float)});
    EXPECT_EQ(tail->GetDesc().offset, stride);
    EXPECT_EQ(tail->GetDesc().element_stride, stride);
    EXPECT_EQ(last->GetDesc().offset, 2 * stride);
    {
        auto tail_data = tail->GetMappedSpan<float>();
        tail_data[1]   = 11.0f;
    }
    {
        auto last_data = last->GetMappedSpan<float>();
        EXPECT_EQ(last_data.front(), 11.0f);
    }
}

TEST_P(BindlessTest, StorageViewPadsOnlyBindingTailAndChecksBounds) {
    auto device = create_device(GetParam(), "StorageViewBounds");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    auto                        buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                                     .size   = 16,
                                                                                     .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                                 });
    // Three 3-byte records every 5 bytes: payload ends at 13, binding ends at 16.
    auto view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                           .buffer         = buffer,
                                                                           .element_size   = 3,
                                                                           .element_count  = 3,
                                                                           .element_stride = 5,
                                                                       });
    EXPECT_EQ(view->GetDesc().element_stride, 5);
    EXPECT_EQ(view->Size(), 16);
    {
        auto mapped = view->GetMappedSpan<std::array<std::byte, 3>>();
        mapped[2]   = {std::byte{1}, std::byte{2}, std::byte{3}};
    }
    const auto bytes = buffer->Map();
    EXPECT_EQ(bytes[10], std::byte{1});
    EXPECT_EQ(bytes[12], std::byte{3});
    buffer->UnMap();
    EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                            .buffer         = buffer,
                                                                            .element_size   = 4,
                                                                            .element_count  = 2,
                                                                            .element_stride = 3,
                                                                        }),
                 std::invalid_argument);
    EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                            .buffer         = buffer,
                                                                            .element_size   = 4,
                                                                            .element_count  = std::numeric_limits<std::uint64_t>::max(),
                                                                            .element_stride = 8,
                                                                        }),
                 std::invalid_argument);
    EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                            .buffer       = buffer,
                                                                            .offset       = std::numeric_limits<std::uint64_t>::max(),
                                                                            .element_size = 4,
                                                                        }),
                 std::invalid_argument);
    EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                            .buffer       = buffer,
                                                                            .element_size = 0,
                                                                        }),
                 std::invalid_argument);
    auto unaligned_stride = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                                       .buffer         = buffer,
                                                                                       .element_size   = sizeof(float),
                                                                                       .element_count  = 2,
                                                                                       .element_stride = 5,
                                                                                   });
    EXPECT_THROW(unaligned_stride->GetMappedSpan<float>(), std::invalid_argument);
    auto tight_buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                    .size   = 3,
                                                                    .usages = GPUBufferUsageFlags::StorageRead,
                                                                });
    EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                            .buffer       = tight_buffer,
                                                                            .element_size = 3,
                                                                        }),
                 std::invalid_argument);
}

TEST_P(BindlessTest, ReadAndWriteViewsShareStorage) {
    auto device = create_device(GetParam(), "OverlappingReadWriteViews");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    auto                        buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                                     .size   = 12,
                                                                                     .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
                                                                                 });
    auto                        read   = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                                                    .buffer        = buffer,
                                                                                                    .type          = GPUBufferViewType::StorageRead,
                                                                                                    .element_size  = 4,
                                                                                                    .element_count = 3,
                                                                                                });
    auto                        write  = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {
                                                                                                    .buffer        = buffer,
                                                                                                    .type          = GPUBufferViewType::StorageWrite,
                                                                                                    .element_size  = 4,
                                                                                                    .element_count = 3,
                                                                                                });
    EXPECT_EQ(read->GetDesc().offset, write->GetDesc().offset);
    EXPECT_EQ(read->GetDesc().element_stride, write->GetDesc().element_stride);
    EXPECT_EQ(read->Size(), write->Size());
    EXPECT_FALSE(read->GetBindlessHandle().writable);
    EXPECT_TRUE(write->GetBindlessHandle().writable);
}

TEST_P(BindlessTest, RenderGraphMapsVertexAndIndexViews) {
    auto                  device = create_device(GetParam(), "MappedVertexIndexViews");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*device, queues, *bindings);
    const auto            vertices = graph.Create(GPUBufferDesc{
        .size   = 3 * sizeof(float),
        .usages = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::MapWrite,
    });
    const auto            indices  = graph.Create(GPUBufferDesc{
        .size   = 3 * sizeof(std::uint16_t),
        .usages = GPUBufferUsageFlags::Index | GPUBufferUsageFlags::MapWrite,
    });
    const auto            target   = graph.Create(TextureDesc{
        .width  = 1,
        .height = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::RenderTarget,
    });
    bool                  executed = false;
    rg::RenderPassBuilder builder(graph);
    builder.AllowPassCulling(false);
    const auto vertices_access = builder.ReadAsVertices(vertices, {.element_size = sizeof(float), .element_count = 0});
    const auto indices_access  = builder.ReadAsIndices(indices, {.element_size = sizeof(std::uint16_t), .element_count = 0});
    builder.SetRenderTarget(target, true);
    builder.SetExecutor([&](const rg::RenderGraph&, const rg::RenderPassNode& pass) {
        auto& vertex_view = pass.Resolve(vertices_access);
        auto& index_view  = pass.Resolve(indices_access);
        EXPECT_FALSE(vertex_view.GetBindlessHandle());
        EXPECT_FALSE(index_view.GetBindlessHandle());
        EXPECT_EQ(vertex_view.GetDesc().element_stride, sizeof(float));
        EXPECT_EQ(index_view.GetDesc().element_stride, sizeof(std::uint16_t));
        auto vertex_data = vertex_view.GetMappedSpan<float>();
        auto index_data  = index_view.GetMappedSpan<std::uint16_t>();
        ASSERT_EQ(vertex_data.size(), 3);
        ASSERT_EQ(index_data.size(), 3);
        vertex_data[2] = 7.0f;
        index_data[2]  = 2;
        EXPECT_FLOAT_EQ(vertex_data.data()[2], 7.0f);
        EXPECT_EQ(index_data.data()[2], 2);
        executed = true;
    });
    builder.Finish();
    graph.Compile();
    graph.Execute();
    queues.WaitIdle();
    EXPECT_TRUE(executed);
}

TEST_P(BindlessTest, RenderGraphOverlappingViewsAndHandleOwnership) {
    auto                   device = create_device(GetParam(), "GraphAccessViews");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*device, queues, *bindings);
    const auto             buffer  = graph.Create(GPUBufferDesc{.size = 32, .usages = GPUBufferUsageFlags::StorageRead});
    const auto             texture = graph.Create(TextureDesc{
        .width = 8, .height = 8, .depth = 1, .format = Format::R8G8B8A8_UNORM, .mip_levels = 2, .usages = TextureUsageFlags::SRV});
    rg::ComputePassBuilder first(graph);
    first.AllowPassCulling(false);
    const rg::GPUBufferEdgeHandle records  = first.Read(buffer, {.element_size = 4, .element_count = 8});
    const rg::GPUBufferEdgeHandle pairs    = first.Read(buffer, {.element_size = 8, .element_count = 4});
    const rg::TextureEdgeHandle   all_mips = first.Read(texture);
    const rg::TextureEdgeHandle   one_mip  = first.Read(texture, {.base_mip_level = 1, .mip_levels = 1});
    EXPECT_NE(records, pairs);
    EXPECT_NE(all_mips, one_mip);
    bool first_executed = false;
    first.SetExecutor([&](const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        first_executed    = true;
        auto& record_view = pass.Resolve(records);
        auto& pair_view   = pass.Resolve(pairs);
        EXPECT_EQ(&record_view, &pass.Resolve(records));
        EXPECT_NE(&record_view, &pair_view);
        EXPECT_EQ(record_view.GetDesc().buffer, pair_view.GetDesc().buffer);
        EXPECT_EQ(record_view.GetDesc().element_count, 8);
        EXPECT_EQ(pair_view.GetDesc().element_count, 4);
        EXPECT_TRUE(record_view.GetBindlessHandle());
        EXPECT_TRUE(pair_view.GetBindlessHandle());
        EXPECT_NE(record_view.GetBindlessHandle().index, pair_view.GetBindlessHandle().index);
        EXPECT_EQ(pass.Resolve(all_mips).GetDesc().texture, pass.Resolve(one_mip).GetDesc().texture);
        EXPECT_EQ(pass.Resolve(one_mip).GetDesc().base_mip_level, 1);
        EXPECT_THROW(pass.Resolve(rg::GPUBufferEdgeHandle{}), std::out_of_range);
        EXPECT_THROW(pass.Resolve(rg::TextureEdgeHandle{}), std::out_of_range);
    });
    ASSERT_TRUE(graph.IsValid(first.Finish()));

    rg::ComputePassBuilder second(graph);
    second.AllowPassCulling(false);
    const rg::GPUBufferEdgeHandle other_records = second.Read(buffer);
    second.SetExecutor([&](const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        EXPECT_TRUE(pass.Resolve(other_records).GetBindlessHandle());
        EXPECT_THROW(pass.Resolve(records), std::out_of_range);
        EXPECT_THROW(pass.Resolve(all_mips), std::out_of_range);
    });
    ASSERT_TRUE(graph.IsValid(second.Finish()));
    ASSERT_TRUE(graph.Compile());
    graph.Execute();
    EXPECT_TRUE(first_executed);

    // A retired frame's access must not resolve in a new pass, even if slots are reused.
    rg::ComputePassBuilder next_frame(graph);
    next_frame.AllowPassCulling(false);
    const auto next_buffer = graph.Create(GPUBufferDesc{.size = 32, .usages = GPUBufferUsageFlags::StorageRead});
    const auto next_access = next_frame.Read(next_buffer);
    next_frame.SetExecutor([&](const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        EXPECT_TRUE(pass.Resolve(next_access).GetBindlessHandle());
        EXPECT_THROW(pass.Resolve(records), std::out_of_range);
    });
    ASSERT_TRUE(graph.IsValid(next_frame.Finish()));
    ASSERT_TRUE(graph.Compile());
    graph.Execute();
}

TEST_P(BindlessTest, RenderGraphReadsIndexedScalarRecords) {
    auto            device = create_device(GetParam(), "AlignedScalarRecords");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    rg::RenderGraph             graph(*device, queues, *bindings);
    const auto      requirements = GPUBuffer::GetStorageViewRequirements(*device);
    const auto      stride       = 2 * utils::align(sizeof(float), requirements.offset_alignment);
    // No trailing stride padding: inference must still include the final record.
    auto input = hitagi::gfx::GPUBuffer::Create(*device, {.size = 2 * stride + sizeof(float), .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite});
    if (requirements.offset_alignment > sizeof(float)) {
        EXPECT_THROW(hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = input, .offset = sizeof(float), .element_size = sizeof(float)}), std::invalid_argument);
    }
    {
        auto view   = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = input, .element_size = sizeof(float), .element_count = 0, .element_stride = stride});
        auto values = view->GetMappedSpan<float>();
        ASSERT_EQ(values.size(), 3);
        values[0] = -100.0f;
        values[1] = 7.0f;
        values[2] = 11.0f;
    }
    auto output = hitagi::gfx::GPUBuffer::Create(*device, {.size = sizeof(float), .usages = GPUBufferUsageFlags::StorageWrite | GPUBufferUsageFlags::CopySrc});
    struct Arguments {
        BindlessHandle input, output;
        std::uint32_t  stride;
    };
    auto arguments = hitagi::gfx::GPUBuffer::Create(*device, {.size = 2 * sizeof(Arguments), .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite});
    auto shader    = hitagi::gfx::Shader::Create(*device, compiler, {.type = ShaderType::Compute, .entry = "main", .source_code = R"(
        #include "bindless.hlsl"
        struct Arguments { hitagi::SimpleBuffer input; hitagi::RWSimpleBuffer output; uint stride; };
        [numthreads(1, 1, 1)]
        void main() {
            Arguments args = hitagi::load_bindless<Arguments>();
            args.output.store<float>(args.input.load<float>() + args.input.load<float>(args.stride));
        }
    )"});
    ASSERT_TRUE(shader);
    auto                   pipeline        = hitagi::gfx::ComputePipeline::Create(*device, *bindings, {}, shader);
    const auto             input_handle    = graph.Import(input);
    const auto             output_handle   = graph.Import(output);
    const auto             argument_handle = graph.Import(arguments);
    rg::ComputePassBuilder builder(graph);
    builder.AllowPassCulling(false);
    const auto input_access    = builder.Read(input_handle, {.offset = (1) * (stride), .element_size = sizeof(float), .element_count = 2, .element_stride = stride});
    const auto argument_access = builder.Read(argument_handle, {.offset = 0, .element_size = sizeof(Arguments), .element_count = 2});
    const auto output_access   = builder.Write(output_handle, {.offset = 0, .element_size = sizeof(float), .element_count = 1});
    builder.SetExecutor([&](const rg::RenderGraph&, const rg::ComputePassNode& pass) {
        {
            const auto& desc = pass.Resolve(input_access).GetDesc();
            EXPECT_EQ(desc.offset, stride);
            EXPECT_EQ(desc.element_stride, stride);
            EXPECT_EQ(desc.element_count, 2);
            auto values = pass.Resolve(input_access).GetMappedSpan<const float>();
            EXPECT_FLOAT_EQ(values[0], 7.0f);
            EXPECT_FLOAT_EQ(values[1], 11.0f);
        }
        {
            auto data = pass.Resolve(argument_access).GetMappedSpan<Arguments>();
            data[0]   = {.input = pass.Resolve(input_access).GetBindlessHandle(), .output = pass.Resolve(output_access).GetBindlessHandle(), .stride = 0};
            data[1]   = {.input = pass.Resolve(input_access).GetBindlessHandle(), .output = pass.Resolve(output_access).GetBindlessHandle(), .stride = static_cast<std::uint32_t>(stride)};
        }
        auto& cmd = pass.GetCmd();
        cmd.SetPipeline(*pipeline);
        cmd.PushBindlessMetaInfo({.handle = pass.Resolve(argument_access).GetBindlessHandle(), .record_index = 1, .record_stride = sizeof(Arguments)});
        if (GetParam() == Device::Type::Vulkan) {
            static_cast<VulkanComputeCommandBuffer&>(cmd).command_buffer.dispatch(1, 1, 1);
        }
#ifdef _WIN32
        else {
            static_cast<DX12ComputeCommandList&>(cmd).command_list->Dispatch(1, 1, 1);
        }
#endif
        cmd.ResourceBarrier({}, std::array{output->Transition(BarrierAccess::CopySrc, PipelineStage::Copy)});
    });
    builder.Finish();
    ASSERT_TRUE(graph.Compile());
    graph.Execute();
    queues.WaitIdle();

    auto readback = hitagi::gfx::GPUBuffer::Create(*device, {.size = sizeof(float), .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
    auto copy     = hitagi::gfx::CopyCommandContext::Create(*device, queues);
    copy->Begin();
    copy->ResourceBarrier({}, std::array{
                                  output->Transition(BarrierAccess::CopySrc, PipelineStage::Copy),
                                  readback->Transition(BarrierAccess::CopyDst, PipelineStage::Copy)});
    copy->CopyBuffer(*output, 0, *readback, 0, sizeof(float));
    copy->End();
    auto& queue = queues.Get(CommandType::Copy);
    queue.Submit({{*copy}});
    queue.WaitIdle();
    auto readback_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = readback, .element_size = sizeof(float)});
    auto result        = readback_view->GetMappedSpan<const float>();
    EXPECT_FLOAT_EQ(result.front(), 18.0f);
}
