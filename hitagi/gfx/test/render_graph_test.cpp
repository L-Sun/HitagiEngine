#include <hitagi/gfx/render_graph.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;
using namespace hitagi::math;

class RenderGraphTest : public ::testing::Test {
public:
    RenderGraphTest() : device(Device::Create(Device::Type::DX12, ::testing::UnitTest::GetInstance()->current_test_info()->name())) {}
    std::unique_ptr<Device> device;
};

TEST_F(RenderGraphTest, RenderPass) {
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
            {"POSITION", 0, 0, 0, Format::R32G32B32_FLOAT},
            {"COLOR", 0, 1, 3 * sizeof(float), Format::R32G32B32_FLOAT},
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
                {reinterpret_cast<const std::byte*>(triangle.data()), triangle.size() * sizeof(float)}),
            .stride = 2 * sizeof(vec3f),
            .usages = GpuBufferView::UsageFlags::Vertex,
        });

    RenderGraph rg(*device);
    struct ColorPass {
        ResourceHandle output;
    };
    auto color_pass = rg.AddPass<ColorPass>(
        "color",
        [&](RenderGraph::Builder& builder, ColorPass& data) {
            data.output = builder.Create(Texture::Desc{
                .name   = "output",
                .width  = 300,
                .height = 300,
                .format = Format::R8G8B8A8_UNORM,
                .usages = Texture::UsageFlags::RTV,
            });
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
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}