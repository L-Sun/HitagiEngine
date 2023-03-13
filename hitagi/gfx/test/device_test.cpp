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
    Device::Type::DX12,
#endif
    Device::Type::Vulkan,
};
class DeviceTest : public TestWithParam<Device::Type> {
protected:
    DeviceTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(GetParam(), test_name)) {}

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
};
INSTANTIATE_TEST_SUITE_P(
    DeviceTest,
    DeviceTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });
TEST_P(DeviceTest, CreateDevice) {
    ASSERT_TRUE(device != nullptr);
}

TEST_P(DeviceTest, CreateSemaphore) {
    auto semaphore = device->CreateSemaphore(32);
    EXPECT_TRUE(semaphore != nullptr) << "Failed to create semaphore";
}

TEST_P(DeviceTest, CreateCommandContext) {
    auto context = device->CreateGraphicsContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create graphics context";
}

TEST_P(DeviceTest, CreateComputeCommandContext) {
    auto context = device->CreateComputeContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create compute context";
}

TEST_P(DeviceTest, CreateCopyCommandContext) {
    auto context = device->CreateCopyContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create copy context";
}

class SemaphoreTest : public DeviceTest {
protected:
    SemaphoreTest() : semaphore(device->CreateSemaphore()) {}

    std::shared_ptr<Semaphore> semaphore;
};
INSTANTIATE_TEST_SUITE_P(
    SemaphoreTest,
    SemaphoreTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });
TEST_P(SemaphoreTest, GetCurrentValue) {
    EXPECT_EQ(semaphore->GetCurrentValue(), 0) << "The initial value of the semaphore should be 0";
    auto semaphore_2 = device->CreateSemaphore(1);
    EXPECT_EQ(semaphore_2->GetCurrentValue(), 1) << "The initial value of the semaphore should be 1";
}

TEST_P(SemaphoreTest, Signal) {
    EXPECT_EQ(semaphore->GetCurrentValue(), 0) << "The initial value of the semaphore should be 0";
    semaphore->Signal(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(semaphore->GetCurrentValue(), 1) << "The value of the semaphore should be 1 after signal";
}

TEST_P(SemaphoreTest, Wait) {
    auto signal_fn = [this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        semaphore->Signal(1);
    };
    auto wait_fn = [this]() {
        semaphore->Wait(1);
        EXPECT_EQ(semaphore->GetCurrentValue(), 1) << "The value of the semaphore should be 1 after wait";
    };
    std::thread signal_thread(signal_fn);
    std::thread wait_thread(wait_fn);

    wait_thread.join();
    signal_thread.join();

    EXPECT_EQ(semaphore->GetCurrentValue(), 1) << "The value of the semaphore should be 1 after wait";
};

class GpuBufferTest : public TestWithParam<std::tuple<Device::Type, GpuBuffer::UsageFlags>> {
protected:
    GpuBufferTest()
        : test_name(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(std::get<0>(GetParam()), test_name)),
          usages(std::get<1>(GetParam())) {}

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
    GpuBuffer::UsageFlags   usages;
};
INSTANTIATE_TEST_SUITE_P(
    GpuBufferTest,
    GpuBufferTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(
            GpuBuffer::UsageFlags::Vertex,
            GpuBuffer::UsageFlags::Index,
            GpuBuffer::UsageFlags::Constant | GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            GpuBuffer::UsageFlags::MapRead | GpuBuffer::UsageFlags::CopyDst)),
    [](const TestParamInfo<std::tuple<Device::Type, GpuBuffer::UsageFlags>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            flags_name(std::get<1>(info.param)));
    });
TEST_P(GpuBufferTest, Create) {
    constexpr std::string_view initial_data = "abcdefg";

    auto gpu_buffer = device->CreateGpuBuffer(
        {
            .name         = test_name,
            .element_size = initial_data.size(),
            .usages       = usages,
        },
        {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});

    ASSERT_TRUE(gpu_buffer != nullptr);
    EXPECT_EQ(gpu_buffer->GetDesc().element_size, initial_data.size()) << "The size of the buffer must be the same as described";

    if (has_flag(usages, GpuBuffer::UsageFlags::MapRead)) {
        ASSERT_TRUE(gpu_buffer->GetMappedPtr() != nullptr) << "A mapped buffer must contain a mapped pointer";
        EXPECT_STREQ(initial_data.data(), std::string(reinterpret_cast<const char*>(gpu_buffer->GetMappedPtr()), gpu_buffer->GetDesc().element_size).c_str())
            << "The content of the buffer must be the same as initial data";
    }
    if (has_flag(usages, GpuBuffer::UsageFlags::MapWrite)) {
        ASSERT_TRUE(gpu_buffer->GetMappedPtr() != nullptr) << "A mapped buffer must contain a mapped pointer";
        std::string_view new_data = "hello";
        std::memcpy(gpu_buffer->GetMappedPtr(), new_data.data(), new_data.size());
        EXPECT_STREQ(
            (std::string("hello") + std::string("fg")).data(),
            std::string(reinterpret_cast<const char*>(gpu_buffer->GetMappedPtr()), gpu_buffer->GetDesc().element_size).c_str())
            << "The content of the buffer must be the same as initial data";
    }
}

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
INSTANTIATE_TEST_SUITE_P(
    CommandTest,
    CommandTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(CommandType::Graphics, CommandType::Compute, CommandType::Copy)),
    [](const TestParamInfo<std::tuple<Device::Type, CommandType>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            magic_enum::enum_name(std::get<1>(info.param)));
    });
TEST_P(CommandTest, CopyBuffer) {
    constexpr std::string_view initial_data = "abcdefg";

    auto src_buffer = device->CreateGpuBuffer(
        {
            .name         = fmt::format("{}_src", test_name),
            .element_size = initial_data.size(),
            .usages       = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
        });
    auto dst_buffer = device->CreateGpuBuffer(
        {
            .name         = fmt::format("{}_dst", test_name),
            .element_size = initial_data.size(),
            .usages       = GpuBuffer::UsageFlags::MapRead | GpuBuffer::UsageFlags::CopyDst,
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

TEST_P(DeviceTest, CreateTexture) {
    // Create 1D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });
        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->GetDesc().width, 128);
        EXPECT_EQ(texture->GetDesc().height, 1);
        EXPECT_EQ(texture->GetDesc().depth, 1);
    }
    // Create 2D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            },
            {});

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->GetDesc().width, 128);
        EXPECT_EQ(texture->GetDesc().height, 128);
        EXPECT_EQ(texture->GetDesc().depth, 1);
    }
    // Create 3D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .depth       = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->GetDesc().width, 128);
        EXPECT_EQ(texture->GetDesc().height, 128);
        EXPECT_EQ(texture->GetDesc().depth, 128);
    }
    // Create 2D texture array
    {
        auto texture = device->CreateTexture(
            {
                .name        = UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .array_size  = 6,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->GetDesc().width, 128);
        EXPECT_EQ(texture->GetDesc().height, 128);
    }
}

TEST_P(DeviceTest, CreateSampler) {
    auto sampler = device->CreatSampler(

        {
            .name      = UnitTest::GetInstance()->current_test_info()->name(),
            .address_u = Sampler::AddressMode::Repeat,
            .address_v = Sampler::AddressMode::Repeat,
            .address_w = Sampler::AddressMode::Repeat,
            .compare   = CompareFunction::Always,
        });

    ASSERT_TRUE(sampler != nullptr);
}

TEST_P(DeviceTest, CompileShader) {
    // Test Vertex Shader
    {
        Shader vs_shader{
            .name        = "vertex_test_shader",
            .type        = Shader::Type::Vertex,
            .entry       = "VSMain",
            .source_code = R"""(
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
            )""",
        };
        device->CompileShader(vs_shader);
        EXPECT_FALSE(vs_shader.binary_data.Empty());
    }
    {
        Shader ps_shader{
            .name        = "pixel_test_shader",
            .type        = Shader::Type::Pixel,
            .entry       = "PSMain",
            .source_code = R"""(
                struct PS_INPUT{
                    float4 pos : SV_POSITION;
                };

                float4 PSMain(PS_INPUT input) : SV_TARGET {
                    return input.pos;
                }
            )""",
        };
        device->CompileShader(ps_shader);
        EXPECT_FALSE(ps_shader.binary_data.Empty());
    }
}

TEST_P(DeviceTest, CreatePipeline) {
    {
        constexpr auto vs_code = R"""(
            #define RSDEF                                   \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)" \

            struct VS_INPUT {
                float3 pos : POSITION;
            };
            struct PS_INPUT {
                float4 pos : SV_POSITION;
            };
            
            [RootSignature(RSDEF)]
            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = float4(input.pos, 1.0f);
                return output;
            }
        )""";

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name = UnitTest::GetInstance()->current_test_info()->name(),
                .vs   = {
                      .name        = UnitTest::GetInstance()->current_test_info()->name(),
                      .type        = Shader::Type::Vertex,
                      .entry       = "VSMain",
                      .source_code = vs_code,
                },
                .input_layout = {{"POSITION", 0, 0, 0, Format::R32G32B32_FLOAT, false, 0}},

            });
        EXPECT_TRUE(render_pipeline);
    }
}

TEST_P(DeviceTest, CommandContextTest) {
    // Graphics context test
    {
        constexpr auto vs_code = R"""(
            #define RSDEF                                    \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
            "RootConstants(num32BitConstants=4, b0),"        \
            "CBV(b1),"                                       \

            cbuffer PushData : register(b0) {
                float4 push_data;
            };
            cbuffer ConstantData : register(b1) {
                float4 constant_data;
            };

            struct VS_INPUT {
                float3 pos : POSITION;
            };
            struct PS_INPUT {
                float4 pos : SV_POSITION;
            };
            
            [RootSignature(RSDEF)]
            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = float4(input.pos, 1.0f) + push_data + constant_data;
                return output;
            }
        )""";

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name = UnitTest::GetInstance()->current_test_info()->name(),
                .vs   = {
                      .name        = UnitTest::GetInstance()->current_test_info()->name(),
                      .type        = Shader::Type::Vertex,
                      .entry       = "VSMain",
                      .source_code = vs_code,
                },
                .input_layout = {{"POSITION", 0, 0, 0, Format::R32G32B32_FLOAT, false, 0}},
            });
        ASSERT_TRUE(render_pipeline);

        auto constant_buffer = device->CreateGpuBuffer(
            {
                .name         = UnitTest::GetInstance()->current_test_info()->name(),
                .element_size = sizeof(vec4f),
                .usages       = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc | GpuBuffer::UsageFlags::Constant,
            });

        constant_buffer->Update(0, vec4f(1, 2, 3, 4));

        auto graphics_context = device->CreateGraphicsContext(::testing::UnitTest::GetInstance()->current_test_info()->name());
        ASSERT_TRUE(graphics_context);
        EXPECT_NO_THROW({
            graphics_context->SetPipeline(*render_pipeline);
            graphics_context->PushConstant(0, vec4f(1, 2, 3, 4));
            graphics_context->BindConstantBuffer(1, *constant_buffer);
            graphics_context->End();
        });
    }
    // Compute context test
    {
        auto compute_context = device->CreateComputeContext(::testing::UnitTest::GetInstance()->current_test_info()->name());
        EXPECT_TRUE(compute_context);
    }
    // Copy context test
    {
        auto copy_context = device->CreateCopyContext(::testing::UnitTest::GetInstance()->current_test_info()->name());
        ASSERT_TRUE(copy_context);

        constexpr std::string_view initial_data = "abcdefg";

        auto upload_buffer = device->CreateGpuBuffer(
            {
                .name         = fmt::format("Upload-{}", UnitTest::GetInstance()->current_test_info()->name()),
                .element_size = initial_data.size(),
                .usages       = GpuBuffer::UsageFlags::CopySrc | GpuBuffer::UsageFlags::MapWrite,
            },
            {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});
        auto readback_buffer = device->CreateGpuBuffer(
            {
                .name         = fmt::format("Upload-{}", UnitTest::GetInstance()->current_test_info()->name()),
                .element_size = initial_data.size(),
                .usages       = GpuBuffer::UsageFlags::CopyDst | GpuBuffer::UsageFlags::MapRead,
            });

        copy_context->CopyBuffer(*upload_buffer, 0, *readback_buffer, 0, initial_data.size());
        copy_context->End();

        auto& copy_queue = device->GetCommandQueue(CommandType::Copy);
        copy_queue.Submit({copy_context.get()});
        copy_queue.WaitIdle();

        EXPECT_STREQ(reinterpret_cast<const char*>(readback_buffer->GetMappedPtr()), initial_data.data());
    }
}

class SwapChainTest : public DeviceTest {
protected:
    SwapChainTest()
        : DeviceTest(),
          app(hitagi::Application::CreateApp(hitagi::AppConfig{
              .title = std::pmr::string{fmt::format("App/{}", test_name)},
          })) {}

    void SetUp() override {
        ASSERT_TRUE(app != nullptr);
        swap_chain = device->CreateSwapChain({
            .name   = test_name,
            .window = app->GetWindow(),
            .format = device->device_type == Device::Type::Vulkan ? Format::B8G8R8A8_UNORM : Format::R8G8B8A8_UNORM,
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
    EXPECT_EQ(swap_chain->Width(), rect.right - rect.left) << "swap chain should be same size as window";
    EXPECT_EQ(swap_chain->Height(), rect.bottom - rect.top) << "swap chain should be same size as window";
}

TEST_P(SwapChainTest, GetBackBuffer) {
    auto rect = app->GetWindowsRect();

    for (const Texture& back_buffer : swap_chain->GetBuffers()) {
        EXPECT_EQ(back_buffer.GetDesc().width, rect.right - rect.left) << "Each back buffer should have the same size as the swap chain after resizing";
        EXPECT_EQ(back_buffer.GetDesc().height, rect.bottom - rect.top) << "Each back buffer should have the same size as the swap chain after resizing";
        EXPECT_EQ(back_buffer.GetDesc().format, swap_chain->GetDesc().format) << "Each back buffer should have the same format as the swap chain";
        EXPECT_TRUE(hitagi::utils::has_flag(back_buffer.GetDesc().usages, Texture::UsageFlags::RTV)) << "Back buffer should have RTV usage";
    }
}

TEST_P(SwapChainTest, SwapChainResizing) {
    auto rect = app->GetWindowsRect();

    // Resize swap chain
    app->ResizeWindow(800, 600);
    rect = app->GetWindowsRect();
    swap_chain->Resize();

    EXPECT_EQ(swap_chain->Width(), rect.right - rect.left) << "Swap chain should be same size as window after resizing";
    EXPECT_EQ(swap_chain->Height(), rect.bottom - rect.top) << "Swap chain should be same size as window after resizing";

    for (const Texture& buffer : swap_chain->GetBuffers()) {
        EXPECT_EQ(buffer.GetDesc().width, rect.right - rect.left) << "Each buffer should have the same size as the swap chain after resizing";
        EXPECT_EQ(buffer.GetDesc().height, rect.bottom - rect.top) << "Each buffer should have the same size as the swap chain after resizing";
    }
}

TEST_P(DeviceTest, DrawTriangle) {
    auto app = hitagi::Application::CreateApp(
        hitagi::AppConfig{
            .title = std::pmr::string{fmt::format("App/{}", test_name)},
        });
    {
        auto rect = app->GetWindowsRect();

        auto swap_chain = device->CreateSwapChain(
            {
                .name   = UnitTest::GetInstance()->current_test_info()->name(),
                .window = app->GetWindow(),
                .format = Format::R8G8B8A8_UNORM,
            });

        constexpr std::string_view shader_code = R"""(
            #define RSDEF                                                                     \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT),"                                  \
            "RootConstants(num32BitConstants=16, b0,  visibility=SHADER_VISIBILITY_VERTEX)"   \

            cbuffer Rotation : register(b0) {
                matrix rotation;
            };

            struct VS_INPUT {
                float4 pos : POSITION;
                float3 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float3 col : COLOR;
            };

            [RootSignature(RSDEF)]
            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = mul(rotation, input.pos);
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                return float4(input.col, 1.0f);
            }
        )""";

        auto pipeline = device->CreateRenderPipeline({
            .name = "I know DirectX12 pipeline",
            .vs   = {
                  .name        = "I know DirectX12 vertex shader",
                  .type        = Shader::Type::Vertex,
                  .entry       = "VSMain",
                  .source_code = std::pmr::string(shader_code),
            },
            .ps = {
                .name        = "I know DirectX12 pixel shader",
                .type        = Shader::Type::Pixel,
                .entry       = "PSMain",
                .source_code = std::pmr::string(shader_code),
            },
            .input_layout = {
                // clang-format off
                {"POSITION", 0, 0,             0, Format::R32G32B32_FLOAT},
                {   "COLOR", 0, 0, sizeof(vec3f), Format::R32G32B32_FLOAT},
                // clang-format on
            },
            .rasterizer_config = {
                .front_counter_clockwise = false,
            },
        });

        // clang-format off
        constexpr std::array<vec3f, 6> triangle = {{
            /*         pos       */  /*    color     */
            {-0.25f, -0.25f, 0.00f}, {1.0f, 0.0f, 0.0f},  // point 0
            { 0.00f,  0.25f, 0.00f}, {0.0f, 1.0f, 0.0f},  // point 1
            { 0.25f, -0.25f, 0.00f}, {0.0f, 0.0f, 1.0f},  // point 2
        }};
        // clang-format on
        auto vertex_buffer = device->CreateGpuBuffer(
            {
                .name          = "I know DirectX12 positions buffer",
                .element_size  = 2 * sizeof(vec3f),
                .element_count = 3,
                .usages        = GpuBuffer::UsageFlags::Vertex,
            },
            {reinterpret_cast<const std::byte*>(triangle.data()), triangle.size() * sizeof(vec3f)});

        auto& gfx_queue = device->GetCommandQueue(CommandType::Graphics);
        auto  context   = device->CreateGraphicsContext("I know DirectX12 context");

        hitagi::core::Clock timer;
        timer.Start();

        context->SetRenderTarget(swap_chain->GetCurrentBackBuffer());
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
        context->ClearRenderTarget(swap_chain->GetCurrentBackBuffer());
        context->SetPipeline(*pipeline);
        context->SetVertexBuffer(0, *vertex_buffer);

        context->PushConstant(0, rotate_z(deg2rad(90.0f)));

        context->Draw(3);
        context->Present(swap_chain->GetCurrentBackBuffer());
        context->End();

        gfx_queue.Submit({context.get()});
        swap_chain->Present();
        gfx_queue.WaitIdle();
        context->Reset();
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}