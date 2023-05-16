#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;
using namespace hitagi::math;
using namespace testing;

// clang-format off
constexpr std::array<vec3f, 3> triangle = {{
    {-0.25f, -0.25f, 0.00f}, 
    { 0.00f,  0.25f, 0.00f}, 
    { 0.25f, -0.25f, 0.00f}, 
}};
// clang-format on

constexpr std::array supported_device_types = {
    Device::Type::DX12,
    Device::Type::Vulkan,
};

class RenderGraphTest : public TestWithParam<Device::Type> {
protected:
    RenderGraphTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          app(hitagi::Application::CreateApp(
              hitagi::AppConfig{
                  .title = std::pmr::string{fmt::format("App-{}", test_name)},
              })),
          device(Device::Create(GetParam(), fmt::format("Device-{}", test_name))),
          rg(*device, fmt::format("RenderGraph-{}", test_name)) {
    }

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create device";
    }
    std::pmr::string                     test_name;
    std::unique_ptr<hitagi::Application> app;
    std::unique_ptr<Device>              device;
    RenderGraph                          rg;
};

INSTANTIATE_TEST_SUITE_P(
    RenderGraphTest,
    RenderGraphTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(RenderGraphTest, ImportBuffer) {
    auto buffer = device->CreateGPUBuffer({
        .name         = "TestBuffer",
        .element_size = 1024,
        .usages       = GPUBufferUsageFlags::Constant,
    });

    auto buffer_handle = rg.Import(buffer);
    EXPECT_TRUE(buffer_handle);
}

TEST_P(RenderGraphTest, ImportTexture) {
    auto texture = device->CreateTexture({
        .name   = "TestTexture",
        .width  = 1024,
        .height = 1024,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
    });

    auto texture_handle = rg.Import(texture);
    EXPECT_TRUE(texture_handle);
}

TEST_P(RenderGraphTest, ImportSwapChain) {
    auto swap_chain = device->CreateSwapChain({
        .name   = "TestSwapChain",
        .window = app->GetWindow(),
    });

    auto swap_chain_handle = rg.Import(swap_chain);
    EXPECT_TRUE(swap_chain_handle);
}

TEST_P(RenderGraphTest, PassTest) {
    auto swap_chain = device->CreateSwapChain({
        .name   = "TestSwapChain",
        .window = app->GetWindow(),
    });
    auto rect       = app->GetWindowsRect();

    auto swap_chain_handle = rg.Import(swap_chain);

    auto vertex_shader = device->CreateShader({
        .name        = fmt::format("TestVertexShader-{}", test_name),
        .type        = ShaderType::Vertex,
        .entry       = "main",
        .source_code = R"(
            #include "bindless.hlsl"
            struct Bindless {
                hitagi::SimpleBuffer constant;
            };

            struct Constant{
                float4 color[3];
            };

            struct VSInput{
                uint   index    : SV_VertexID;
                float3 position : POSITION;
            };

            struct PSInput{
                float4 position : SV_POSITION;
                float4 color : COLOR;
            };

            PSInput main(VSInput input) {
                const Bindless bindless = hitagi::load_bindless<Bindless>();
                Constant constant = bindless.constant.load<Constant>();

                PSInput output;
                output.position = float4(input.position, 1.0f);
                output.color = constant.color[input.index];
                return output;
            }
        )",
    });
    auto pixel_shader  = device->CreateShader({
         .name        = fmt::format("TestPixelShader-{}", test_name),
         .type        = ShaderType::Pixel,
         .entry       = "main",
         .source_code = R"(
            struct PSInput{
                float4 position : SV_POSITION;
                float4 color    : COLOR;
            };

            float4 main(PSInput input) : SV_TARGET {
                return input.color;
            }
        )",
    });

    auto pipeline = device->CreateRenderPipeline({
        .name                = fmt::format("TestPipeline-{}", test_name),
        .shaders             = {vertex_shader, pixel_shader},
        .vertex_input_layout = {
            {"POSITION", 0, Format::R32G32B32_FLOAT, 0, 0, sizeof(vec3f), false},
        },
        .render_format = swap_chain->GetFormat(),
    });

    auto vertices_buffer = rg.Import(
        device->CreateGPUBuffer({
                                    .name          = "Triangle",
                                    .element_size  = sizeof(vec3f),
                                    .element_count = triangle.size(),
                                    .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
                                },
                                {reinterpret_cast<const std::byte*>(triangle.data()), sizeof(vec3f) * triangle.size()}));

    auto constant_buffer = rg.Create(GPUBufferDesc{
        .name          = "TestBuffer",
        .element_size  = sizeof(vec4f),
        .element_count = 3,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    struct Bindless {
        BindlessHandle constant;
    };

    auto bindless_info_buffer = rg.Create(GPUBufferDesc{
        .name         = "BindlessInfo",
        .element_size = sizeof(Bindless),
        .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    struct PassData {
        ResourceHandle vertices;
        ResourceHandle bindless;
        ResourceHandle constant;
        ResourceHandle output;
    };
    auto render_pass = rg.AddPass<PassData, CommandType::Graphics>(
        "TestPass",
        [&](auto& builder, PassData& data) {
            data.vertices = builder.Read(vertices_buffer);
            data.bindless = builder.Read(bindless_info_buffer);
            data.constant = builder.Read(constant_buffer);
            data.output   = builder.SetRenderTarget(swap_chain_handle);
        },
        [=](const ResourceHelper& helper, const PassData& data, GraphicsCommandContext& context) {
            context.SetViewPort(ViewPort{
                .x      = 0,
                .y      = 0,
                .width  = static_cast<float>(rect.right - rect.left),
                .height = static_cast<float>(rect.bottom - rect.top),
            });
            context.SetScissorRect(hitagi::gfx::Rect{
                .x      = rect.left,
                .y      = rect.top,
                .width  = rect.right - rect.left,
                .height = rect.bottom - rect.top,
            });

            auto constant_buffer = helper.Get<GPUBuffer>(data.constant).Span<vec4f>();
            constant_buffer[0]   = vec4f(1.0f, 0.00f, 0.00f, 1.0f);
            constant_buffer[1]   = vec4f(0.0f, 1.00f, 0.00f, 1.0f);
            constant_buffer[2]   = vec4f(0.0f, 0.00f, 1.00f, 1.0f);

            auto& bindless_info_buffer                    = helper.Get<GPUBuffer>(data.bindless);
            bindless_info_buffer.Get<Bindless>().constant = helper.Get(data.constant);

            context.SetPipeline(*pipeline);
            context.PushBindlessMetaInfo(BindlessMetaInfo{.handle = helper.Get(data.bindless)});

            context.SetVertexBuffer(0, helper.Get<GPUBuffer>(vertices_buffer));

            context.Draw(3);
        });

    rg.AddPresentPass(render_pass.output);

    EXPECT_TRUE(render_pass.constant);
    EXPECT_TRUE(render_pass.output);

    rg.Compile();

    while (!app->IsQuit()) {
        rg.Execute();
        swap_chain->Present();
        app->Tick();
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}