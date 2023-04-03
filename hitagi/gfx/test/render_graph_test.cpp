#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;
using namespace hitagi::math;
using namespace testing;

constexpr std::array supported_device_types = {
#ifdef _WIN32
    Device::Type::DX12,
#endif
    Device::Type::Vulkan,
};

class RenderGraphTest : public TestWithParam<Device::Type> {
public:
    RenderGraphTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(GetParam(), test_name)) {}

    void SetUp() final {
        ASSERT_TRUE(device != nullptr);
    }

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
};
INSTANTIATE_TEST_SUITE_P(
    RenderGraphTest,
    RenderGraphTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(RenderGraphTest, RenderPass) {
    auto app = hitagi::Application::CreateApp(hitagi::AppConfig{
        .title = std::pmr::string{fmt::format("App-{}", test_name)},
    });
    {
        auto swap_chain = device->CreateSwapChain(
            {
                .name   = fmt::format("swap-chain-{}", test_name),
                .window = app->GetWindow(),
            });

        constexpr std::string_view shader_code = R"""(
            #include "bindless.hlsl"

            struct VS_INPUT {
                float4 pos : POSITION;
                float3 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float3 col : COLOR;
            };

            PS_INPUT VSMain(VS_INPUT input) {
                PS_INPUT output;
                output.pos = input.pos;
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                return float4(input.col, 1.0f);
            }
        )""";

        auto vs = device->CreateShader({
            .name        = fmt::format("vertex-shader-{}", test_name),
            .type        = ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = shader_code,
        });
        auto ps = device->CreateShader({
            .name        = fmt::format("pixel-shader-{}", test_name),
            .type        = ShaderType::Pixel,
            .entry       = "PSMain",
            .source_code = shader_code,
        });

        auto pipeline = device->CreateRenderPipeline({
            .name                = fmt::format("pipeline-{}", test_name),
            .shaders             = {vs, ps},
            .vertex_input_layout = {
                // clang-format off
                {"POSITION", 0, Format::R32G32B32_FLOAT, 0,             0, 2*sizeof(vec3f)},
                {   "COLOR", 0, Format::R32G32B32_FLOAT, 0, sizeof(vec3f), 2*sizeof(vec3f)},
                // clang-format on
            },
            .rasterization_state = {
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
        auto vertex_buffer = device->CreateGPUBuffer(
            {
                .name          = "triangle",
                .element_size  = 2 * sizeof(vec3f),
                .element_count = 3,
                .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
            },
            {reinterpret_cast<const std::byte*>(triangle.data()), triangle.size() * sizeof(vec3f)});

        RenderGraph rg(*device);

        auto vertex_buffer_handle = rg.Import("vertex_buffer", vertex_buffer);
        auto swap_chain_handle    = rg.ImportWithoutLifeTrack("swap_chain", swap_chain.get());

        struct ColorPass {
            ResourceHandle vertices;
            ResourceHandle output;
        };
        auto color_pass = rg.AddPass<ColorPass>(
            "color",
            [&](RenderGraph::Builder& builder, ColorPass& data) {
                data.vertices = builder.Read(vertex_buffer_handle);
                data.output   = builder.Write(swap_chain_handle);
            },
            [=](const RenderGraph::ResourceHelper& helper, const ColorPass& data, GraphicsCommandContext* context) {
                auto& render_target = helper.Get<SwapChain>(data.output);

                context->SetPipeline(*pipeline);
                context->BeginRendering({
                    .render_target = render_target,
                });
                context->SetViewPort(ViewPort{
                    .x      = 0,
                    .y      = 0,
                    .width  = static_cast<float>(render_target.GetWidth()),
                    .height = static_cast<float>(render_target.GetHeight()),
                });
                context->SetScissorRect(hitagi::gfx::Rect{
                    .x      = 0,
                    .y      = 0,
                    .width  = render_target.GetWidth(),
                    .height = render_target.GetHeight(),
                });
                context->SetVertexBuffer(0, helper.Get<GPUBuffer>(data.vertices));
                context->Draw(3);
            });

        rg.PresentPass(color_pass.output);
        EXPECT_TRUE(rg.Compile());
        rg.Execute();

        swap_chain->Present();
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}