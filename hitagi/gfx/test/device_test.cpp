#include <hitagi/core/buffer.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>
#include <hitagi/utils/flags.hpp>

using namespace hitagi::core;
using namespace hitagi::gfx;
using namespace hitagi::math;
using namespace hitagi::utils;

using namespace testing;

namespace hitagi::gfx {
std::ostream& operator<<(std::ostream& os, const Device::Type& type) {
    return os << magic_enum::enum_name(type);
}
template <utils::EnumFlag E>
std::string flags_name(E flags) {
    auto name = magic_enum::enum_flags_name(flags);
    std::replace(name.begin(), name.end(), '|', '_');
    return name;
}
}  // namespace hitagi::gfx

constexpr std::array supported_device_types = {
#ifdef _WIN32
// Device::Type::DX12,
#endif
    Device::Type::Vulkan,
};
class CreateTest : public TestWithParam<Device::Type> {
protected:
    CreateTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(GetParam(), test_name)) {}

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
};
INSTANTIATE_TEST_SUITE_P(
    CreateTest,
    CreateTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });
TEST_P(CreateTest, CreateDevice) {
    ASSERT_TRUE(device != nullptr);
}

TEST_P(CreateTest, CreateGraphicsCommandContext) {
    auto context = device->CreateGraphicsContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create graphics context";
}

TEST_P(CreateTest, CreateComputeCommandContext) {
    auto context = device->CreateComputeContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create compute context";
}

TEST_P(CreateTest, CreateCopyCommandContext) {
    auto context = device->CreateCopyContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create copy context";
}

TEST_P(CreateTest, CreateTexture1D) {
    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        });
    EXPECT_TRUE(texture != nullptr);
}

TEST_P(CreateTest, CreateTexture2D) {
    Buffer data(128 * 128 * sizeof(vec4f));

    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .height      = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
        },
        data.Span<const std::byte>());

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(CreateTest, CreateTexture3D) {
    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .height      = 128,
            .depth       = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        });

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(CreateTest, CreateTexture2DArray) {
    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .height      = 128,
            .array_size  = 6,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        });

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(CreateTest, CreateSampler) {
    auto sampler = device->CreatSampler(
        {
            .name       = test_name,
            .address_u  = AddressMode::Repeat,
            .address_v  = AddressMode::Repeat,
            .address_w  = AddressMode::Repeat,
            .compare_op = CompareOp::Always,
        });

    ASSERT_TRUE(sampler != nullptr);
}

TEST_P(CreateTest, CreateShader) {
    constexpr std::string_view shader_code = R"""(
        cbuffer cb : register(b0, space0) {
            matrix mvp;
        };
        cbuffer cb : register(b0, space1) {
            matrix mvp2;
        };
        Texture2D<float4> materialTextures[4] : register(t0);

        struct VS_INPUT {
            float3 pos : POSITION;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
        };
        
        PS_INPUT VSMain(VS_INPUT input) {
            PS_INPUT output;
            output.pos = mul(mvp, float4(input.pos, 1.0f));
            return output;
        }
        float4 PSMain(VS_INPUT input) : SV_TARGET {
            PS_INPUT output;
            return float4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    )""";

    // Test Vertex Shader
    {
        auto vs_shader = device->CreateShader({
            .name        = "vertex_test_shader",
            .type        = ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = shader_code,
        });
        switch (device->device_type) {
            case Device::Type::DX12:
                EXPECT_FALSE(vs_shader->GetDXILData().empty()) << "When using DirectX 12, the shader must be compile to DXIL";
                break;
            case Device::Type::Vulkan:
                EXPECT_FALSE(vs_shader->GetSPIRVData().empty()) << "When using Vulkan, the shader must be compile to SPIRV";
                break;
            default:
                EXPECT_FALSE(true) << "Would not happen";
        }
    }
    {
        auto ps_shader = device->CreateShader({
            .name        = "pixel_test_shader",
            .type        = ShaderType::Pixel,
            .entry       = "PSMain",
            .source_code = shader_code,
        });
        switch (device->device_type) {
            case Device::Type::DX12:
                EXPECT_FALSE(ps_shader->GetDXILData().empty()) << "When using DirectX 12, the shader must be compile to DXIL";
                break;
            case Device::Type::Vulkan:
                EXPECT_FALSE(ps_shader->GetSPIRVData().empty()) << "When using Vulkan, the shader must be compile to SPIRV";
                break;
            default:
                EXPECT_FALSE(true) << "Would not happen";
        }
    }
}

TEST_P(CreateTest, CreateRenderPipeline) {
    {
        constexpr auto vs_code = R"""(
            struct VS_INPUT {
                float3 pos : POSITION;
            };
            struct PS_INPUT {
                float4 pos : SV_POSITION;
            };
            
            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = float4(input.pos, 1.0f);
                return output;
            }
        )""";

        auto vs_shader = device->CreateShader({
            .name        = test_name,
            .type        = ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = vs_code,
        });
        ASSERT_TRUE(vs_shader);

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name                = test_name,
                .shaders             = {vs_shader},
                .vertex_input_layout = {
                    {"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0, 0},
                },
            });
        EXPECT_TRUE(render_pipeline);
    }
}

TEST_P(CreateTest, CreateComputPipeline) {
    constexpr auto cs_code = R"""(
        #include "bindless.hlsl"
        RWStructuredBuffer<float> output : register(u0);

        [numthreads(1, 1, 1)]
        void main(uint3 thread_id : SV_DispatchThreadID) {
            output[thread_id.x] = 1.0f;
        }
    )""";

    auto cs_shader = device->CreateShader({
        .name        = test_name,
        .type        = ShaderType::Compute,
        .entry       = "main",
        .source_code = cs_code,
    });
    ASSERT_TRUE(cs_shader);

    auto compute_pipeline = device->CreateComputePipeline({
        .name = test_name,
        .cs   = cs_shader,
    });
    EXPECT_TRUE(compute_pipeline);
}

class GPUBufferTest : public TestWithParam<std::tuple<Device::Type, GPUBufferUsageFlags>> {
protected:
    GPUBufferTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(std::get<0>(GetParam()), test_name)),
          usages(std::get<1>(GetParam())) {}

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
    GPUBufferUsageFlags     usages;
};

INSTANTIATE_TEST_SUITE_P(
    GPUBufferTest,
    GPUBufferTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(
            GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
            GPUBufferUsageFlags::Index | GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
            GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
            GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::CopySrc | GPUBufferUsageFlags::CopyDst,
            GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
            GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst)),
    [](const TestParamInfo<std::tuple<Device::Type, GPUBufferUsageFlags>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            flags_name(std::get<1>(info.param)));
    });

TEST_P(GPUBufferTest, Create) {
    constexpr std::string_view initial_data = "abcdefg";

    auto gpu_buffer = device->CreateGPUBuffer(
        {
            .name         = test_name,
            .element_size = initial_data.size(),
            .usages       = usages,
        },
        {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});

    ASSERT_TRUE(gpu_buffer != nullptr);
    EXPECT_EQ(gpu_buffer->GetDesc().element_size, initial_data.size()) << "The size of the buffer must be the same as described";

    if (has_flag(usages, GPUBufferUsageFlags::MapRead)) {
        ASSERT_TRUE(gpu_buffer->GetMappedPtr() != nullptr) << "A mapped buffer must contain a mapped pointer";
        EXPECT_STREQ(initial_data.data(), std::string(reinterpret_cast<const char*>(gpu_buffer->GetMappedPtr()), gpu_buffer->GetDesc().element_size).c_str())
            << "The content of the buffer must be the same as initial data";
    }
    if (has_flag(usages, GPUBufferUsageFlags::MapWrite)) {
        ASSERT_TRUE(gpu_buffer->GetMappedPtr() != nullptr) << "A mapped buffer must contain a mapped pointer";
        std::string_view new_data = "hello";
        std::memcpy(gpu_buffer->GetMappedPtr(), new_data.data(), new_data.size());
        EXPECT_STREQ(
            (std::string("hello") + std::string("fg")).data(),
            std::string(reinterpret_cast<const char*>(gpu_buffer->GetMappedPtr()), gpu_buffer->GetDesc().element_size).c_str())
            << "The content of the buffer must be the same as initial data";
    }
}

class FenceTest : public CreateTest {
protected:
    FenceTest() : fence(device->CreateFence()) {}

    std::shared_ptr<Fence> fence;
};
INSTANTIATE_TEST_SUITE_P(
    FenceTest,
    FenceTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });
TEST_P(FenceTest, GetCurrentValue) {
    EXPECT_EQ(fence->GetCurrentValue(), 0) << "The initial value of the fence should be 0";
    auto fence_2 = device->CreateFence(1);
    EXPECT_EQ(fence_2->GetCurrentValue(), 1) << "The initial value of the fence should be 1";
}

TEST_P(FenceTest, Signal) {
    EXPECT_EQ(fence->GetCurrentValue(), 0) << "The initial value of the fence should be 0";
    fence->Signal(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after signal";
}

TEST_P(FenceTest, Wait) {
    auto signal_fn = [this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fence->Signal(1);
    };
    auto wait_fn = [this]() {
        fence->Wait(1);
        EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after wait";
    };
    std::thread signal_thread(signal_fn);
    std::thread wait_thread(wait_fn);

    wait_thread.join();
    signal_thread.join();

    EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after wait";
};

class CommandTest : public TestWithParam<std::tuple<Device::Type, CommandType>> {
protected:
    CommandTest() : test_name(UnitTest::GetInstance()->current_test_info()->name()),
                    device(Device::Create(std::get<0>(GetParam()), test_name)) {
        switch (std::get<1>(GetParam())) {
            case CommandType::Graphics:
                context = device->CreateGraphicsContext();
                break;
            case CommandType::Compute:
                context = device->CreateComputeContext();
                break;
            case CommandType::Copy:
                context = device->CreateCopyContext();
                break;
            default:
                break;
        }
    }

    void SetUp() override {
        ASSERT_TRUE(context) << fmt::format("Failed to create command context: {}", magic_enum::enum_name(std::get<1>(GetParam())));
    }

    std::pmr::string                test_name;
    std::unique_ptr<Device>         device;
    std::shared_ptr<CommandContext> context;
};

class CopyCommandTest : public CommandTest {
protected:
    using CommandTest::CommandTest;
};
INSTANTIATE_TEST_SUITE_P(
    CopyCommandTest,
    CopyCommandTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(CommandType::Graphics, CommandType::Compute, CommandType::Copy)),
    [](const TestParamInfo<std::tuple<Device::Type, CommandType>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            magic_enum::enum_name(std::get<1>(info.param)));
    });

TEST_P(CopyCommandTest, CopyBuffer) {
    constexpr std::string_view initial_data = "abcdefg";

    auto src_buffer = device->CreateGPUBuffer(
        {
            .name         = fmt::format("{}_src", test_name),
            .element_size = initial_data.size(),
            .usages       = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
        });
    auto dst_buffer = device->CreateGPUBuffer(
        {
            .name         = fmt::format("{}_dst", test_name),
            .element_size = initial_data.size(),
            .usages       = GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst,
        });
    ASSERT_TRUE(src_buffer != nullptr);
    ASSERT_TRUE(dst_buffer != nullptr);

    std::memcpy(src_buffer->GetMappedPtr(), initial_data.data(), initial_data.size());

    if (context->GetType() == CommandType::Copy) {
        auto& copy_queue = device->GetCommandQueue(CommandType::Copy);

        auto ctx = std::static_pointer_cast<CopyCommandContext>(context);

        ctx->Begin();
        ctx->CopyBuffer(*src_buffer, 0, *dst_buffer, 0, src_buffer->GetDesc().element_size);
        ctx->End();

        copy_queue.Submit({ctx.get()});
        copy_queue.WaitIdle();

        EXPECT_STREQ(initial_data.data(), std::string(reinterpret_cast<const char*>(dst_buffer->GetMappedPtr()), dst_buffer->GetDesc().element_size).c_str())
            << "The content of the buffer must be the same as initial data";
    }
}

class BindlessTest : public CommandTest {
protected:
    using CommandTest::CommandTest;
};
INSTANTIATE_TEST_SUITE_P(
    BindlessTest,
    BindlessTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(CommandType::Graphics, CommandType::Compute)),
    [](const TestParamInfo<std::tuple<Device::Type, CommandType>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            magic_enum::enum_name(std::get<1>(info.param)));
    });

TEST_P(BindlessTest, PushBindlessInfo) {
    auto rotation     = rotate_z(90.0_deg);
    auto frame_buffer = device->CreateGPUBuffer(
        {
            .name         = fmt::format("{}_buffer", test_name),
            .element_size = sizeof(rotation),
            .usages       = GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(&rotation), sizeof(rotation)});
    ASSERT_TRUE(frame_buffer != nullptr);

    struct BindlessInfo {
        BindlessHandle frame_buffer_handle;
    } bindless_info;
    bindless_info.frame_buffer_handle = device->GetBindlessUtils().CreateBindlessHandle(*frame_buffer);

    auto bindless_info_buffer = device->CreateGPUBuffer({
        .name         = fmt::format("{}_bindless_info_buffer", test_name),
        .element_size = sizeof(BindlessInfo),
        .usages       = GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::MapWrite,
    });
    bindless_info_buffer->Update(0, bindless_info);

    BindlessInfoOffset bindless_info_offset{
        .bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*bindless_info_buffer),
    };

    if (context->GetType() == CommandType::Graphics) {
        constexpr std::string_view vs_shader_code = R"""(
            #include "bindless.hlsl"

            struct Bindless {
                hitagi::SimpleBuffer frame_constant;
            };

            struct FrameConstant {
                matrix mvp;
                matrix proj;
            };

            static const float2 positions[3] = {
                float2(0.0f, 0.5f),
                float2(0.5f, -0.5f),
                float2(-0.5f, -0.5f)
            };

            float4 main(uint index: SV_VertexID) : SV_Position {
                Bindless      resource       = hitagi::load_bindless<Bindless>();
                FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();
                return mul(frame_constant.mvp, float4(positions[index], 0.0f, 1.0f));
            }
        )""";

        auto vs_shader = device->CreateShader({
            .name        = fmt::format("{}-vs", test_name),
            .type        = ShaderType::Vertex,
            .entry       = "main",
            .source_code = vs_shader_code,
        });
        ASSERT_TRUE(vs_shader);

        auto texture = device->CreateTexture(TextureDesc{
            .name   = fmt::format("{}-texture", test_name),
            .width  = 10,
            .height = 10,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::RTV,
        });
        ASSERT_TRUE(texture);

        auto pipeline = device->CreateRenderPipeline({
            .name          = fmt::format("{}-pipeline", test_name),
            .shaders       = {vs_shader},
            .render_format = texture->GetDesc().format,
        });
        ASSERT_TRUE(pipeline);

        auto& queue   = device->GetCommandQueue(context->GetType());
        auto  gfx_ctx = std::static_pointer_cast<GraphicsCommandContext>(context);

        gfx_ctx->Begin();

        gfx_ctx->ResourceBarrier(
            {}, {},
            {
                TextureBarrier{
                    .src_access = BarrierAccess::Unkown,
                    .dst_access = BarrierAccess::RenderTarget,
                    .src_stage  = PipelineStage::None,
                    .dst_stage  = PipelineStage::Render,
                    .src_layout = TextureLayout::Unkown,
                    .dst_layout = TextureLayout::RenderTarget,
                    .texture    = *texture,
                },
            });

        gfx_ctx->SetPipeline(*pipeline);
        gfx_ctx->PushBindlessInfo(bindless_info_offset);
        gfx_ctx->BeginRendering({
            .render_target = *texture,
        });
        gfx_ctx->SetViewPort({.x = 0, .y = 0, .width = 10, .height = 10});
        gfx_ctx->SetScissorRect({.x = 0, .y = 0, .width = 10, .height = 10});
        gfx_ctx->Draw(3);
        gfx_ctx->EndRendering();
        gfx_ctx->End();

        queue.Submit({context.get()});
        queue.WaitIdle();

    } else if (context->GetType() == CommandType::Compute) {
        constexpr std::string_view cs_shader_code = R"""(
            #include "bindless.hlsl"
            struct Bindless {
                hitagi::SimpleBuffer cb;
            };

            struct Constant {
                float value;
            };

            [numthreads(1, 1, 1)]
            void main() {
                Bindless bindless = hitagi::load_bindless<Bindless>();
                Constant constant = bindless.cb.load<Constant>();
            }
        )""";

        auto cs_shader = device->CreateShader({
            .name        = fmt::format("{}-cs", test_name),
            .type        = ShaderType::Compute,
            .entry       = "main",
            .source_code = cs_shader_code,
        });
        ASSERT_TRUE(cs_shader);

        auto pipeline = device->CreateComputePipeline({
            .name = fmt::format("{}-pipeline", test_name),
            .cs   = cs_shader,
        });
        ASSERT_TRUE(pipeline);

        auto& queue           = device->GetCommandQueue(context->GetType());
        auto  compute_context = std::static_pointer_cast<ComputeCommandContext>(context);

        compute_context->Begin();

        compute_context->SetPipeline(*pipeline);
        compute_context->PushBindlessInfo(bindless_info_offset);
        // TODO dispatch command
        compute_context->End();

        queue.Submit({context.get()});
        queue.WaitIdle();
    } else {
        FAIL() << "Unsupported command type";
    }

    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info_offset.bindless_info_handle);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.frame_buffer_handle);
}

class SwapChainTest : public CreateTest {
protected:
    SwapChainTest()
        : CreateTest(),
          app(hitagi::Application::CreateApp(hitagi::AppConfig{
              .title = std::pmr::string{fmt::format("App/{}", test_name)},
          })) {}

    void SetUp() override {
        ASSERT_TRUE(app != nullptr);
        swap_chain = device->CreateSwapChain({
            .name   = test_name,
            .window = app->GetWindow(),
        });
        ASSERT_TRUE(swap_chain != nullptr);
    }

    std::unique_ptr<hitagi::Application> app;
    std::shared_ptr<SwapChain>           swap_chain;
};
INSTANTIATE_TEST_SUITE_P(
    SwapChainTest,
    SwapChainTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(SwapChainTest, CreateSwapChain) {
    auto rect = app->GetWindowsRect();
    EXPECT_EQ(swap_chain->GetWidth(), rect.right - rect.left) << "swap chain should be same size as window";
    EXPECT_EQ(swap_chain->GetHeight(), rect.bottom - rect.top) << "swap chain should be same size as window";
}

TEST_P(SwapChainTest, GetBackBuffers) {
    auto rect = app->GetWindowsRect();

    for (const Texture& back_texture : swap_chain->GetTextures()) {
        EXPECT_EQ(back_texture.GetDesc().width, rect.right - rect.left) << "Each back texture should have the same size as the swap chain after resizing";
        EXPECT_EQ(back_texture.GetDesc().height, rect.bottom - rect.top) << "Each back texture should have the same size as the swap chain after resizing";
        EXPECT_EQ(back_texture.GetDesc().format, swap_chain->GetFormat()) << "Each back texture should have the same format as the swap chain";
        EXPECT_TRUE(hitagi::utils::has_flag(back_texture.GetDesc().usages, TextureUsageFlags::RTV)) << "Back texture should have RTV usage";
    }
}

TEST_P(SwapChainTest, SwapChainResizing) {
    auto rect = app->GetWindowsRect();

    // Resize swap chain
    app->ResizeWindow(800, 600);
    rect = app->GetWindowsRect();
    swap_chain->Resize();

    EXPECT_EQ(swap_chain->GetWidth(), rect.right - rect.left) << "Swap chain should be same size as window after resizing";
    EXPECT_EQ(swap_chain->GetHeight(), rect.bottom - rect.top) << "Swap chain should be same size as window after resizing";

    for (const Texture& buffer : swap_chain->GetTextures()) {
        EXPECT_EQ(buffer.GetDesc().width, rect.right - rect.left) << "Each buffer should have the same size as the swap chain after resizing";
        EXPECT_EQ(buffer.GetDesc().height, rect.bottom - rect.top) << "Each buffer should have the same size as the swap chain after resizing";
    }
}

TEST_P(CreateTest, DrawTriangle) {
    auto app = hitagi::Application::CreateApp(
        hitagi::AppConfig{
            .title = std::pmr::string{fmt::format("App/{}", test_name)},
        });

    auto rect = app->GetWindowsRect();

    auto swap_chain = device->CreateSwapChain(
        {
            .name   = UnitTest::GetInstance()->current_test_info()->name(),
            .window = app->GetWindow(),
        });

    constexpr std::string_view shader_code = R"""(
            #include "bindless.hlsl"

            struct Bindless {
                hitagi::SimpleBuffer cb;
            };

            struct Constant {
                matrix rotation;
            };
        
            struct VS_INPUT {
                float3 pos : POSITION;
                float3 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float3 col : COLOR;
            };

            PS_INPUT VSMain(VS_INPUT input) {
                Bindless bindless = hitagi::load_bindless<Bindless>();
                Constant constant = bindless.cb.load<Constant>();

                PS_INPUT output;
                output.pos = mul(constant.rotation, float4(input.pos, 1.0f));
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                return float4(input.col, 1.0f);
            }
        )""";

    auto vertex_shader = device->CreateShader({
        .name        = fmt::format("VS-{}", test_name),
        .type        = ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(vertex_shader);

    auto pixel_shader = device->CreateShader({
        .name        = fmt::format("PS-{}", test_name),
        .type        = ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(pixel_shader);

    auto pipeline = device->CreateRenderPipeline({
        .name    = "I know DirectX12 pipeline",
        .shaders = {
            vertex_shader,
            pixel_shader,
        },
        .vertex_input_layout = {
            {"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0, 2 * sizeof(vec3f)},
            {"COLOR", 0, Format::R32G32B32_FLOAT, 0, sizeof(vec3f), 2 * sizeof(vec3f)},
        },
        .rasterization_state = {
            .front_counter_clockwise = false,
        },
        .render_format = swap_chain->GetFormat(),
    });

    // clang-format off
        constexpr std::array<vec3f, 6> triangle = {{
            /*         pos       */  /*    color     */
            {-0.25f, -0.25f, 0.00f}, {1.0f, 0.0f, 0.0f},  // point 0
            { 0.00f,  0.25f, 0.00f}, {0.0f, 1.0f, 0.0f},  // point 1
            { 0.25f, -0.25f, 0.00f}, {0.0f, 0.0f, 1.0f},  // point 2
        }};
    // clang-format on
    auto vertex_buffer = device->CreateGPUBuffer(
        {
            .name          = "I know DirectX12 positions buffer",
            .element_size  = 2 * sizeof(vec3f),
            .element_count = 3,
            .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(triangle.data()), triangle.size() * sizeof(vec3f)});

    struct Constant {
        mat4f rotation;
    };
    auto constant_buffer = device->CreateGPUBuffer({
        .name          = fmt::format("{}-ConstantBuffer", test_name),
        .element_size  = sizeof(Constant),
        .element_count = 1,
        .usages        = GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::MapWrite,
    });
    constant_buffer->Update(0, rotate_z(20.0_degf));

    auto bindless_info_buffer = device->CreateGPUBuffer({
        .name          = fmt::format("{}-BindlessHandles", test_name),
        .element_size  = sizeof(BindlessInfoOffset),
        .element_count = 1,
        .usages        = GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::MapWrite,
    });
    bindless_info_buffer->Update(
        0,
        BindlessInfoOffset{
            .bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*constant_buffer),
        });

    BindlessInfoOffset bindless_info_offset{
        .bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*bindless_info_buffer),
    };

    auto& gfx_queue = device->GetCommandQueue(CommandType::Graphics);
    auto  context   = device->CreateGraphicsContext("I know DirectX12 context");

    context->Begin();
    context->SetPipeline(*pipeline);
    context->SetViewPort(ViewPort{
        .x      = 0,
        .y      = 0,
        .width  = static_cast<float>(rect.right - rect.left),
        .height = static_cast<float>(rect.bottom - rect.top),
    });
    context->SetScissorRect(hitagi::gfx::Rect{
        .x      = rect.left,
        .y      = rect.top,
        .width  = rect.right - rect.left,
        .height = rect.bottom - rect.top,
    });
    context->SetVertexBuffer(0, *vertex_buffer);

    context->BeginRendering({
        .render_target = *swap_chain,
    });
    context->PushBindlessInfo(bindless_info_offset);
    context->Draw(3);
    context->EndRendering();
    context->Present(*swap_chain);
    context->End();

    gfx_queue.Submit({context.get()});
    swap_chain->Present();
    gfx_queue.WaitIdle();
}

#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}