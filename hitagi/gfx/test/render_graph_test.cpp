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

    struct PassData {
        ResourceHandle vertices;
        ResourceHandle bindless;
        ResourceHandle constant;
        ResourceHandle output;
    };
    struct Bindless {
        BindlessHandle constant;
    };
    auto render_pass = rg.AddPass<PassData, CommandType::Graphics>(
        "TestPass",
        [&](auto& builder, PassData& data) {
            data.vertices = builder.Read(vertices_buffer);
            data.bindless = builder.Read(rg.Create(GPUBufferDesc{
                .name          = "BindlessInfo",
                .element_size  = sizeof(Bindless),
                .element_count = 3,
                .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
            }));
            data.constant = builder.Read(rg.Create(GPUBufferDesc{
                .name          = "TestBuffer",
                .element_size  = sizeof(vec4f),
                .element_count = 3,
                .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
            }));
            data.output   = builder.Write(rg.Import(swap_chain), PipelineStage::Render);
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

            auto constant_buffer = GPUBufferView<vec4f>(helper.Get<GPUBuffer>(data.constant));
            constant_buffer[0]   = vec4f(1.0f, 0.00f, 0.00f, 1.0f);
            constant_buffer[1]   = vec4f(0.0f, 1.00f, 0.00f, 1.0f);
            constant_buffer[2]   = vec4f(0.0f, 0.00f, 1.00f, 1.0f);

            auto bindless_infos        = GPUBufferView<Bindless>(helper.Get<GPUBuffer>(data.bindless));
            bindless_infos[0].constant = helper.GetCBV(data.constant, 0);
            bindless_infos[1].constant = helper.GetCBV(data.constant, 1);
            bindless_infos[2].constant = helper.GetCBV(data.constant, 2);

            std::pmr::vector<GPUBufferBarrier> gpu_buffer_barriers;
            for (auto handle : {data.constant, data.bindless}) {
                gpu_buffer_barriers.emplace_back(GPUBufferBarrier{
                    .src_access = BarrierAccess::None,
                    .dst_access = BarrierAccess::Constant,
                    .src_stage  = PipelineStage::None,
                    .dst_stage  = PipelineStage::VertexShader,
                    .buffer     = helper.Get<GPUBuffer>(handle),

                });
            }
            gpu_buffer_barriers.emplace_back(GPUBufferBarrier{
                .src_access = BarrierAccess::None,
                .dst_access = BarrierAccess::Vertex,
                .src_stage  = PipelineStage::None,
                .dst_stage  = PipelineStage::VertexInput,
                .buffer     = helper.Get<GPUBuffer>(vertices_buffer),
            });

            context.ResourceBarrier(
                {},
                gpu_buffer_barriers,
                {TextureBarrier{
                    .src_access = BarrierAccess::None,
                    .dst_access = BarrierAccess::RenderTarget,
                    .src_stage  = PipelineStage::Render,
                    .dst_stage  = PipelineStage::Render,
                    .src_layout = TextureLayout::Unkown,
                    .dst_layout = TextureLayout::RenderTarget,
                    .texture    = helper.Get<Texture>(data.output),
                }});

            context.BeginRendering(helper.Get<Texture>(data.output));
            {
                context.SetPipeline(*pipeline);
                context.SetVertexBuffer(0, helper.Get<GPUBuffer>(vertices_buffer));

                context.PushBindlessMetaInfo({.handle = helper.GetCBV(data.bindless, 0)});
                context.Draw(3);

                context.PushBindlessMetaInfo({.handle = helper.GetCBV(data.bindless, 1)});
                context.Draw(3);

                context.PushBindlessMetaInfo({.handle = helper.GetCBV(data.bindless, 2)});
                context.Draw(3);
            }
            context.EndRendering();
        });

    rg.AddPresentPass(render_pass.output);

    EXPECT_TRUE(render_pass.constant);
    EXPECT_TRUE(render_pass.output);

    rg.Compile();
    rg.Execute();
    swap_chain->Present();
    app->Tick();
    rg.GetDevice().WaitIdle();
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}