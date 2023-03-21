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

TEST_P(CreateTest, CreateFence) {
    auto fence = device->CreateFence();
    EXPECT_TRUE(fence != nullptr) << "Failed to create fence";
}

TEST_P(CreateTest, CreateSemaphore) {
    auto semaphore = device->CreateSemaphore();
    EXPECT_TRUE(semaphore != nullptr) << "Failed to create semaphore";
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
    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .height      = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        },
        {});

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
            .name      = test_name,
            .address_u = AddressMode::Repeat,
            .address_v = AddressMode::Repeat,
            .address_w = AddressMode::Repeat,
            .compare   = CompareOp::Always,
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

TEST_P(CreateTest, CreateRootSignature) {
    constexpr std::string_view vs_shader_code = R"""(
        struct PushConstant {
            float value;
        };
        struct FrameConstant {
            matrix mvp;
        };

        [[vk::push_constant]]
        ConstantBuffer<PushConstant>     push_constant;
        ConstantBuffer<FrameConstant>    frame_constant : register(b0, space0);

        struct VS_INPUT {
            float3 pos : POSITION;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
        };
        PS_INPUT VSMain(VS_INPUT input) {
            PS_INPUT output;
            output.pos = mul(frame_constant.mvp, float4(input.pos, push_constant.value));
            return output;
        }
    )""";

    constexpr std::string_view ps_shader_code = R"""(
        struct PushConstant {
            float value;
        };
        struct MaterialConstant {
            float3 diffuse;
        };
        [[vk::push_constant]]
        ConstantBuffer<PushConstant>     push_constant;
        ConstantBuffer<MaterialConstant> material    : register(b0, space1);
        Texture2D<float4>                textures[4] : register(t0);

        struct PS_INPUT {
            float4 pos : SV_POSITION;
        };
        float4 PSMain(PS_INPUT input) : SV_TARGET {
            PS_INPUT output;
            return float4(material.diffuse, push_constant.value);
        }
    )""";

    auto vs_shader = device->CreateShader({
        .name        = "vertex_test_shader",
        .type        = ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = vs_shader_code,
    });
    ASSERT_TRUE(vs_shader);

    auto ps_shader = device->CreateShader({
        .name        = "pixel_test_shader",
        .type        = ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = ps_shader_code,
    });
    ASSERT_TRUE(ps_shader);

    auto root_signature = device->CreateRootSignature({
        .name    = "test_root_signature",
        .shaders = {vs_shader, ps_shader},
    });
    EXPECT_TRUE(root_signature);
}

TEST_P(CreateTest, CreatePipeline) {
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

        auto root_signature = device->CreateRootSignature({
            .name    = test_name,
            .shaders = {vs_shader},
        });
        ASSERT_TRUE(root_signature);

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name                = test_name,
                .shaders             = {vs_shader},
                .root_signature      = root_signature,
                .vertex_input_layout = {
                    {"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0, 0},
                },
            });
        EXPECT_TRUE(render_pipeline);
    }
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

class BindCommandTest : public CommandTest {
protected:
    using CommandTest::CommandTest;
};
INSTANTIATE_TEST_SUITE_P(
    BindCommandTest,
    BindCommandTest,
    Combine(
        ValuesIn(supported_device_types),
        Values(CommandType::Graphics, CommandType::Compute)),
    [](const TestParamInfo<std::tuple<Device::Type, CommandType>>& info) -> std::string {
        return fmt::format(
            "{}_{}",
            magic_enum::enum_name(std::get<0>(info.param)),
            magic_enum::enum_name(std::get<1>(info.param)));
    });
TEST_P(BindCommandTest, PushConstant) {
    constexpr std::string_view vs_shader_code = R"""(
        struct Constant {
            float value;
        };
        
        [[vk::push_constant]]
        ConstantBuffer<Constant> value;

        float4 main() : POSITION {
            return float4(value.value, 0.0, 0.0, 1.0);
        }
    )""";

    auto vs_shader = device->CreateShader({
        .name        = fmt::format("{}-vs", test_name),
        .type        = ShaderType::Vertex,
        .entry       = "main",
        .source_code = vs_shader_code,
    });
    ASSERT_TRUE(vs_shader);
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

TEST_P(SwapChainTest, AcquireNextTexture) {
    auto rect = app->GetWindowsRect();

    auto fence = device->CreateFence(fmt::format("{}-Fence", test_name));
    ASSERT_TRUE(fence);
    auto&& [texture, index] = swap_chain->AcquireNextTexture({}, *fence);
    fence->Wait();

    EXPECT_EQ(texture.get().GetDesc().width, rect.right - rect.left) << "texture should have the same size as the swap chain after resizing";
    EXPECT_EQ(texture.get().GetDesc().height, rect.bottom - rect.top) << "texture should have the same size as the swap chain after resizing";
    EXPECT_EQ(texture.get().GetDesc().format, swap_chain->GetFormat()) << "texture should have the same format as the swap chain";
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
            struct VS_INPUT {
                float3 pos : POSITION;
                float3 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float3 col : COLOR;
            };

            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = float4(input.pos, 1.0f);
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

    auto root_signature = device->CreateRootSignature({
        .name    = fmt::format("RootSignature-{}", test_name),
        .shaders = {vertex_shader, pixel_shader},
    });
    ASSERT_TRUE(root_signature);

    auto pipeline = device->CreateRenderPipeline({
        .name    = "I know DirectX12 pipeline",
        .shaders = {
            vertex_shader,
            pixel_shader,
        },
        .root_signature      = root_signature,
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

    auto swapchain_semaphore                = device->CreateSemaphore("swapchain semaphore");
    auto draw_semaphore                     = device->CreateSemaphore("draw semaphore");
    auto [render_target, back_buffer_index] = swap_chain->AcquireNextTexture(*swapchain_semaphore);

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

    // Render target barrier
    context->ResourceBarrier(
        {}, {},
        {
            TextureBarrier{
                .src_access = BarrierAccess::Unkown,
                .dst_access = BarrierAccess::RenderTarget,
                .src_stage  = BarrierStage::None,
                .dst_stage  = BarrierStage::Render,
                .src_layout = BarrierLayout::Unkown,
                .dst_layout = BarrierLayout::RenderTarget,
                .texture    = render_target,
            },
        });

    context->BeginRendering({
        .render_target             = render_target,
        .render_target_clear_value = vec4f{0.0f, 0.0f, 0.0f, 1.0f},
    });
    context->Draw(3);
    context->EndRendering();

    context->ResourceBarrier(
        {}, {},
        {
            TextureBarrier{
                .src_access = BarrierAccess::RenderTarget,
                .dst_access = BarrierAccess::Present,
                .src_stage  = BarrierStage::Render,
                .dst_stage  = BarrierStage::None,
                .src_layout = BarrierLayout::RenderTarget,
                .dst_layout = BarrierLayout::Present,
                .texture    = render_target,
            },
        });

    context->End();

    gfx_queue.Submit({context.get()}, {*swapchain_semaphore}, {*draw_semaphore});
    swap_chain->Present(back_buffer_index, {*draw_semaphore});
    gfx_queue.WaitIdle();
}

#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}