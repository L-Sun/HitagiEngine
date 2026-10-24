#include "test_macros.hpp"
import std;
import interop.gtest;
import interop.vulkan;
import gfx;
import gfx.vulkan;
import app;
import math;
import utils;

using namespace hitagi;
using namespace hitagi::gfx;
using namespace hitagi::utils;

TEST(VulkanQueueTest, NativeStateIsSharedAcrossGroupsAndConcurrentSubmissions) {
    auto device = create_device(Device::Type::Vulkan, "SharedNativeQueue");
    CommandQueues first(*device), second(*device);
    constexpr std::array types{CommandType::Graphics, CommandType::Compute, CommandType::Copy};
    for (const auto type : types) {
        EXPECT_EQ(static_cast<VulkanCommandQueue&>(first.Get(type)).GetNativeQueue(),
                  static_cast<VulkanCommandQueue&>(second.Get(type)).GetNativeQueue());
    }
    auto& backend = static_cast<VulkanDevice&>(*device);
    if (backend.GetPhysicalDevice().getQueueFamilyProperties()[backend.GetQueueFamilyIndex()].queueCount == 1) {
        EXPECT_EQ(backend.GetNativeQueue(CommandType::Graphics), backend.GetNativeQueue(CommandType::Compute));
        EXPECT_EQ(backend.GetNativeQueue(CommandType::Graphics), backend.GetNativeQueue(CommandType::Copy));
    }
    std::array<std::shared_ptr<Fence>, 6> fences;
    for (auto& fence : fences) fence = Fence::Create(*device);
    std::array<std::exception_ptr, 6> errors;
    std::pmr::vector<std::jthread> submitters;
    for (std::size_t index = 0; index < fences.size(); ++index) {
        submitters.emplace_back([&, index] {
            try {
                auto& queue = (index < 3 ? first : second).Get(types[index % 3]);
                for (std::uint64_t value = 1; value <= 64; ++value) {
                    queue.Submit({}, {}, {{FenceSignalInfo{.fence = *fences[index], .value = value}}});
                }
            } catch (...) {
                errors[index] = std::current_exception();
            }
        });
    }
    submitters.clear();
    for (const auto& error : errors) ASSERT_FALSE(error);
    for (auto& fence : fences) fence->Wait(64);
}

TEST(VulkanQueueTest, ComputeTextureToGraphicsToCopy) {
    auto device = create_device(Device::Type::Vulkan, "TextureQueueChain");
    CommandQueues queues(*device);
    auto bindings = BindlessUtils::Create(*device);
    ShaderCompiler compiler{"TextureQueueChain"};
    auto texture = Texture::Create(*device, queues, *bindings, {
        .width = 1, .height = 1, .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::UAV,
    });
    auto storage = TextureView::Create(*device, *bindings, {.texture = texture, .type = TextureViewType::ShaderWrite});
    auto sampled = TextureView::Create(*device, *bindings, {.texture = texture, .type = TextureViewType::ShaderRead});
    auto sampler = Sampler::Create(*device, *bindings, {});
    const std::array handles{storage->GetBindlessHandle(), sampled->GetBindlessHandle(), sampler->GetBindlessHandle()};
    auto arguments = GPUBuffer::Create(*device, {
        .size = sizeof(handles), .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
    }, std::as_bytes(std::span(handles)));
    auto argument_view = GPUBufferView::Create(*device, *bindings, {.buffer = arguments, .element_size = sizeof(handles)});
    const std::pmr::string source = R"(
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
    auto cs = Shader::Create(*device, compiler, {.type = ShaderType::Compute, .entry = "cs_main", .program = {.modules = {{.source_code = source}}}});
    auto vs = Shader::Create(*device, compiler, {.type = ShaderType::Vertex, .entry = "vs_main", .program = {.modules = {{.source_code = source}}}});
    auto ps = Shader::Create(*device, compiler, {.type = ShaderType::Pixel, .entry = "ps_main", .program = {.modules = {{.source_code = source}}}});
    auto compute_pipeline = ComputePipeline::Create(*device, *bindings, {}, cs);
    auto graphics_pipeline = RenderPipeline::Create(*device, *bindings, {}, {vs, ps});
    auto target = Texture::Create(*device, queues, *bindings, {
        .width = 4, .height = 4, .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    });
    auto target_view = TextureView::Create(*device, *bindings, {.texture = target, .type = TextureViewType::RenderTarget});
    auto readback = GPUBuffer::Create(*device, {.size = 64, .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead});
    auto compute = ComputeCommandContext::Create(*device, queues, *bindings);
    compute->Begin();
    compute->ResourceBarrier({}, {}, std::array{texture->Transition(BarrierAccess::ShaderWrite, TextureLayout::ShaderWrite, PipelineStage::ComputeShader)});
    compute->SetPipeline(*compute_pipeline);
    compute->PushBindlessMetaInfo({.handle = argument_view->GetBindlessHandle()});
    static_cast<VulkanComputeCommandBuffer&>(*compute).command_buffer.dispatch(1, 1, 1);
    compute->End();
    auto graphics = GraphicsCommandContext::Create(*device, queues, *bindings);
    graphics->Begin();
    graphics->ResourceBarrier({}, {}, std::array{
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
    auto compute_done = Fence::Create(*device);
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

TEST(VulkanQueueTest, PresentAndRecreateWithSharedQueue) {
    auto application = Application::CreateApp({.width = 64, .height = 64, .gfx_backend = "Vulkan", .headless = true});
    auto device = create_device(Device::Type::Vulkan, "SharedQueuePresent");
    CommandQueues queues(*device);
    auto bindings = BindlessUtils::Create(*device);
    auto swapchain = SwapChain::Create(*device, queues.Get(CommandType::Graphics), {.window = application->GetWindow()});
    for (int frame = 0; frame < 4; ++frame) {
        if (frame == 2) swapchain->Resize();
        auto acquired = swapchain->AcquireTextureForRendering();
        ASSERT_TRUE(acquired);
        auto texture = std::shared_ptr<Texture>(&acquired.value().get(), [](Texture*) {});
        auto view = TextureView::Create(*device, *bindings, {.texture = texture, .type = TextureViewType::RenderTarget});
        auto context = GraphicsCommandContext::Create(*device, queues, *bindings);
        context->Begin();
        context->ResourceBarrier({}, {}, std::array{texture->Transition(BarrierAccess::RenderTarget, TextureLayout::RenderTarget)});
        context->BeginRendering(*view, {}, true);
        context->EndRendering();
        context->ResourceBarrier({}, {}, std::array{texture->Transition(BarrierAccess::Present, TextureLayout::Present)});
        context->End();
        queues.Get(CommandType::Graphics).Submit({{*context}});
        swapchain->Present();
        queues.WaitIdle();
    }
}

TEST(VulkanSurfaceFormatTest, DisplayEncodedOutputUsesUnormRegardlessOfEnumerationOrder) {
    const std::array formats{
        vk::SurfaceFormatKHR{.format = vk::Format::eB8G8R8A8Srgb, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
        vk::SurfaceFormatKHR{.format = vk::Format::eR8G8B8A8Unorm, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
        vk::SurfaceFormatKHR{.format = vk::Format::eB8G8R8A8Unorm, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
    };
    const auto selected = choose_surface_format(formats);
    EXPECT_EQ(selected.format, vk::Format::eB8G8R8A8Unorm);
    EXPECT_EQ(selected.colorSpace, vk::ColorSpaceKHR::eSrgbNonlinear);
    EXPECT_EQ(choose_surface_format(std::span(formats).first(2)).format, vk::Format::eR8G8B8A8Unorm);
    EXPECT_THROW(choose_surface_format(std::span(formats).first(1)), std::runtime_error);
    EXPECT_THROW(choose_surface_format({}), std::runtime_error);
    const std::array unrestricted{
        vk::SurfaceFormatKHR{.format = vk::Format::eUndefined, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear},
    };
    EXPECT_EQ(choose_surface_format(unrestricted).format, vk::Format::eB8G8R8A8Unorm);
}
