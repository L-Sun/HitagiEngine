#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;
using namespace hitagi::math;

class RenderGraphTest : public ::testing::Test {
public:
    RenderGraphTest() : device(Device::Create(Device::Type::DX12, ::testing::UnitTest::GetInstance()->current_test_info()->name())) {}
    std::unique_ptr<Device> device;
};

TEST_F(RenderGraphTest, RenderPass) {
    auto app = hitagi::Application::CreateApp();
    app->Initialize();
    {
        auto swap_chain = device->CreateSwapChain(
            {
                .name       = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .window_ptr = app->GetWindow(),
                .format     = Format::R8G8B8A8_UNORM,
            });
        auto back_buffer = swap_chain->GetCurrentBackBuffer();

        constexpr std::string_view shader_code = R"""(
            #define RSDEF                                    \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)"  \

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
                output.pos = input.pos;
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                return float4(input.col, 1.0f);
            }
        )""";

        auto vs_shader = device->CompileShader(
            {
                .name        = "I know DirectX12 vertex shader",
                .type        = Shader::Type::Vertex,
                .entry       = "VSMain",
                .source_code = shader_code,
            });
        auto ps_shader = device->CompileShader(
            {
                .name        = "I know DirectX12 pixel shader",
                .type        = Shader::Type::Pixel,
                .entry       = "PSMain",
                .source_code = shader_code,
            });
        auto pipeline = device->CreateRenderPipeline({
            .name         = "I know DirectX12 pipeline",
            .vs           = vs_shader,
            .ps           = ps_shader,
            .input_layout = {
                // clang-format off
                {"POSITION", 0, 0,             0, Format::R32G32B32_FLOAT},
                {   "COLOR", 0, 1, sizeof(vec3f), Format::R32G32B32_FLOAT},
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

        auto vertex_buffer = device->CreateBufferView(
            {
                .buffer = device->CreateBuffer(
                    {
                        .name = "triangle",
                        .size = sizeof(vec3f) * triangle.size(),
                    },
                    {reinterpret_cast<const std::byte*>(triangle.data()), triangle.size() * sizeof(vec3f)}),
                .stride = 2 * sizeof(vec3f),
                .usages = GpuBufferView::UsageFlags::Vertex,
            });

        RenderGraph rg(*device);

        auto back_buffer_handle = rg.Import("back_buffer", back_buffer);

        struct ColorPass {
            ResourceHandle output;
        };
        auto color_pass = rg.AddPass<ColorPass>(
            "color",
            [&](RenderGraph::Builder& builder, ColorPass& data) {
                data.output = builder.Write(back_buffer_handle);
            },
            [=](const RenderGraph::ResourceHelper& helper, const ColorPass& data, GraphicsCommandContext* context) {
                const auto& render_target = helper.Get<Texture>(data.output);

                auto rtv = device->CreateTextureView(
                    {
                        .textuer = render_target,
                        .format  = render_target->desc.format,
                    });

                context->SetPipeline(*pipeline);
                context->SetRenderTarget(*rtv);
                context->SetViewPort(ViewPort{
                    .x      = 0,
                    .y      = 0,
                    .width  = static_cast<float>(render_target->desc.width),
                    .height = static_cast<float>(render_target->desc.height),
                });
                context->SetScissorRect(hitagi::gfx::Rect{
                    .x      = 0,
                    .y      = 0,
                    .width  = render_target->desc.width,
                    .height = render_target->desc.height,
                });
                context->SetVertexBuffer(0, *vertex_buffer);
                context->SetVertexBuffer(1, *vertex_buffer);
                context->Draw(3);
            });

        rg.PresentPass(color_pass.output);
        EXPECT_TRUE(rg.Compile());
        rg.Execute();

        swap_chain->Present();
    }
    app->Finalize();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}