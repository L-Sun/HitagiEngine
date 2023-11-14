#include <hitagi/application.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/render_graph/render_graph.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::gfx;
using namespace hitagi::rg;
using namespace hitagi::math;
using namespace testing;

class RenderGraphTest : public Test {
protected:
    RenderGraphTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          device(Device::Create(Device::Type::DX12, test_name)),
          rg(*device, test_name) {
    }

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create device";
    }

    std::string             test_name;
    std::shared_ptr<Device> device;
    RenderGraph             rg;
};

TEST_F(RenderGraphTest, ImportBuffer) {
    const auto buffer_0 = device->CreateGPUBuffer({
        .name          = std::pmr::string(fmt::format("Buffer-{}", test_name)),
        .element_size  = sizeof(float),
        .element_count = 16,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    // import with empty name
    const auto buffer_handle_0 = rg.Import(buffer_0);
    EXPECT_TRUE(rg.IsValid(buffer_handle_0)) << "Import buffer with empty name should succeed";
    EXPECT_EQ(rg.Import(buffer_0, "new_name"), buffer_handle_0) << "Reimport buffer with new name should return same handle";

    const auto buffer_1 = device->CreateGPUBuffer({
        .name          = std::pmr::string(fmt::format("Buffer-{}", test_name)),
        .element_size  = sizeof(float),
        .element_count = 16,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    const auto buffer_handle_1 = rg.Import(buffer_1, buffer_1->GetName());
    EXPECT_TRUE(rg.IsValid(buffer_handle_1)) << "Import buffer with name should succeed";
    EXPECT_EQ(rg.Import(buffer_1, buffer_1->GetName()), buffer_handle_1) << "Reimport buffer with same name should return same handle";
    EXPECT_EQ(rg.Import(buffer_1, "alias_name"), buffer_handle_1) << "Reimport buffer with alias name should return same handle";

    // Fail cases
    {
        std::shared_ptr<GPUBuffer> null_buffer = nullptr;

        EXPECT_FALSE(rg.IsValid(rg.Import(null_buffer))) << "Import nullptr should fail";
        EXPECT_FALSE(rg.IsValid(rg.Import(null_buffer, "null_buffer"))) << "Import nullptr should fail";

        const auto diff_buffer = device->CreateGPUBuffer({
            .name          = std::pmr::string(fmt::format("Buffer-{}", test_name)),
            .element_size  = sizeof(float),
            .element_count = 16,
            .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
        });
        EXPECT_FALSE(rg.IsValid(rg.Import(diff_buffer, buffer_1->GetName()))) << "Import buffer with existed name but different buffer should fail";
    }
}

TEST_F(RenderGraphTest, ImportTexture) {
    const auto texture_0 = device->CreateTexture({
        .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
        .width  = 16,
        .height = 16,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV,
    });
    // import with empty name
    const auto texture_handle_0 = rg.Import(texture_0);
    EXPECT_TRUE(rg.IsValid(texture_handle_0)) << "Import texture with empty name should succeed";
    EXPECT_EQ(rg.Import(texture_0, "new_name"), texture_handle_0) << "Reimport texture with new name should return same handle";

    const auto texture_1 = device->CreateTexture({
        .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
        .width  = 16,
        .height = 16,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV,
    });

    const auto texture_handle_1 = rg.Import(texture_1, texture_1->GetName());
    EXPECT_TRUE(rg.IsValid(texture_handle_1)) << "Import texture with unique name should succeed";
    EXPECT_EQ(rg.Import(texture_1, texture_1->GetName()), texture_handle_1) << "Reimport texture with same name should return same handle";
    EXPECT_EQ(rg.Import(texture_1, "alias_name"), texture_handle_1) << "Reimport texture with alias name should return same handle";

    // Fail cases
    {
        std::shared_ptr<Texture> null_texture = nullptr;

        EXPECT_FALSE(rg.IsValid(rg.Import(null_texture))) << "Import nullptr should fail";
        EXPECT_FALSE(rg.IsValid(rg.Import(null_texture, "null_texture"))) << "Import nullptr should fail";

        const auto diff_texture = device->CreateTexture({
            .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
            .width  = 16,
            .height = 16,
            .depth  = 1,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::SRV,
        });
        EXPECT_FALSE(rg.IsValid(rg.Import(diff_texture, texture_1->GetName()))) << "Import texture with existed name but different texture should fail";
    }
}

TEST_F(RenderGraphTest, CreateTexture) {
    const auto texture_handle = rg.Create(
        {
            .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
            .width  = 16,
            .height = 16,
            .depth  = 1,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::SRV,
        },
        "texture");
    ASSERT_TRUE(rg.IsValid(texture_handle)) << "Create texture should succeed";

    EXPECT_EQ(rg.GetTextureHandle("texture"), texture_handle);
}

TEST_F(RenderGraphTest, MoveBuffer) {
    const auto buffer_0 = device->CreateGPUBuffer({
        .name          = std::pmr::string(fmt::format("Buffer-{}", test_name)),
        .element_size  = sizeof(float),
        .element_count = 16,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    const auto buffer_handle_0 = rg.Import(buffer_0);
    ASSERT_TRUE(rg.IsValid(buffer_handle_0));

    const auto buffer_handle_1 = rg.MoveFrom(buffer_handle_0);
    EXPECT_TRUE(rg.IsValid(buffer_handle_1)) << "Move buffer should succeed";
    EXPECT_NE(buffer_handle_0, buffer_handle_1) << "Move buffer should return different handle";
}

TEST_F(RenderGraphTest, MoveTexture) {
    const auto texture_0 = device->CreateTexture({
        .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
        .width  = 16,
        .height = 16,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV,
    });

    const auto texture_handle_0 = rg.Import(texture_0);
    ASSERT_TRUE(rg.IsValid(texture_handle_0));

    const auto texture_handle_1 = rg.MoveFrom(texture_handle_0);
    EXPECT_TRUE(rg.IsValid(texture_handle_1)) << "Move texture should succeed";
    EXPECT_NE(texture_handle_0, texture_handle_1) << "Move texture should return different handle";
}

TEST_F(RenderGraphTest, AddRenderPass) {
    if (device->device_type == hitagi::gfx::Device::Type::Mock) {
        GTEST_SKIP();
    }

    auto app = hitagi::Application::CreateApp();

    auto swap_chain = device->CreateSwapChain({
        .window = app->GetWindow(),
    });

    const auto vertex_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("VS-{}", test_name)),
        .type        = ShaderType::Vertex,
        .entry       = "vs_main",
        .source_code = R"""(
            struct VSInput{
                float3 position : POSITION;
                float3 color    : COLOR;
            };

            struct VSOutput{
                float4 position : SV_POSITION;
                float4 color    : COLOR;
            };

            VSOutput vs_main(VSInput input) {
                VSOutput output;
                output.position = float4(input.position, 1.0);
                output.color    = float4(input.color,    1.0);
                return output;
            }
        )""",
    });
    ASSERT_TRUE(vertex_shader);

    const auto pixel_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("PS-{}", test_name)),
        .type        = ShaderType::Pixel,
        .entry       = "ps_main",
        .source_code = R"""(
            struct PSInput{
                float4 position : SV_POSITION;
                float4 color    : COLOR;
            };

            float4 ps_main(PSInput input) : SV_TARGET {
                return input.color;
            }
        )""",
    });
    ASSERT_TRUE(pixel_shader);

    const auto vertex_input_layout = device->GetShaderCompiler().ExtractVertexLayout(vertex_shader->GetDesc());
    const auto pipeline            = device->CreateRenderPipeline({
                   .name                = std::pmr::string(fmt::format("Pipeline-{}", test_name)),
                   .shaders             = {vertex_shader, pixel_shader},
                   .vertex_input_layout = vertex_input_layout,
    });

    const auto render_pass =
        RenderPassBuilder(rg)
            .SetName("RenderPass")
            .ReadAsVertices(rg.Create(
                {
                    .name          = std::pmr::string(fmt::format("positions-{}", test_name)),
                    .element_size  = sizeof(vec3f),
                    .element_count = 3,
                    .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::MapWrite,
                },
                "positions"))
            .ReadAsVertices(rg.Create(
                {
                    .name          = std::pmr::string(fmt::format("colors-{}", test_name)),
                    .element_size  = sizeof(vec3f),
                    .element_count = 3,
                    .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::MapWrite,
                },
                "colors"))
            .SetRenderTarget(rg.Create(
                                 {
                                     .name        = std::pmr::string(fmt::format("Texture-{}-{}", test_name, rg.GetFrameIndex())),
                                     .width       = swap_chain->GetWidth(),
                                     .height      = swap_chain->GetHeight(),
                                     .format      = Format::R8G8B8A8_UNORM,
                                     .clear_value = vec4f(0.0, 0.0, 0.0, 1.0),
                                     .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
                                 },
                                 "output"),
                             true)
            .AddPipeline(rg.Import(pipeline, "pipeline"))
            .SetExecutor([=](const RenderGraph& rg, const RenderPassNode& pass) {
                auto rotate_matrix = rotate_z(deg2rad(static_cast<float>(rg.GetFrameIndex())));

                auto&                position_buffer = pass.Resolve(rg.GetBufferHandle("positions"));
                GPUBufferView<vec3f> positions(position_buffer);
                positions[0] = (rotate_matrix * vec4f{0.0f, 0.5f, 0.0f, 1.0f}).xyz;
                positions[1] = (rotate_matrix * vec4f{0.5f, -0.5f, 0.0f, 1.0f}).xyz;
                positions[2] = (rotate_matrix * vec4f{-0.5f, -0.5f, 0.0f, 1.0f}).xyz;

                auto&                color_buffer = pass.Resolve(rg.GetBufferHandle("colors"));
                GPUBufferView<vec3f> colors(color_buffer);
                colors[0] = {1.0f, 0.0f, 0.0f};
                colors[1] = {0.0f, 1.0f, 0.0f};
                colors[2] = {0.0f, 0.0f, 1.0f};

                auto& cmd = pass.GetCmd();
                cmd.SetPipeline(pass.Resolve(rg.GetRenderPipelineHandle("pipeline")));
                cmd.SetViewPort({
                    .x      = 0,
                    .y      = 0,
                    .width  = static_cast<float>(swap_chain->GetWidth()),
                    .height = static_cast<float>(swap_chain->GetHeight()),
                });
                cmd.SetScissorRect({
                    .x      = 0,
                    .y      = 0,
                    .width  = swap_chain->GetWidth(),
                    .height = swap_chain->GetHeight(),
                });

                cmd.SetVertexBuffers(
                    0,
                    {{
                        position_buffer,
                        color_buffer,
                    }},
                    {{0, 0}});
                cmd.Draw(3);
            })
            .Finish();

    EXPECT_TRUE(render_pass);

    PresentPassBuilder(rg)
        .From(rg.GetTextureHandle("output"))
        .SetSwapChain(swap_chain)
        .Finish();

    EXPECT_TRUE(rg.Compile());
    device->Profile(rg.Execute());
    rg.Profile();

    swap_chain->Present();
    app->Tick();
}

TEST_F(RenderGraphTest, GraphTest) {
    if (device->device_type != hitagi::gfx::Device::Type::Mock) {
        GTEST_SKIP();
    }

    const auto render_pipeline  = rg.Import(device->CreateRenderPipeline({}));
    const auto compute_pipeline = rg.Import(device->CreateComputePipeline({}));

    const auto buffer_1 = rg.Create(GPUBufferDesc{
        .name          = "buffer_1",
        .element_size  = sizeof(float),
        .element_count = 1,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::MapWrite,
    });

    const auto buffer_2 = rg.Create(GPUBufferDesc{
        .name          = "buffer_2",
        .element_size  = sizeof(float),
        .element_count = 1,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::Storage | GPUBufferUsageFlags::MapWrite,
    });

    const auto texture_1 = rg.Create(TextureDesc{
        .name   = "texture_1",
        .width  = 512,
        .height = 512,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget,
    });

    const auto texture_2 = rg.Create(TextureDesc{
        .name   = "texture_2",
        .width  = 512,
        .height = 512,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget,
    });

    const auto texture_3 = rg.MoveFrom(texture_1, "texture_3");

    const auto texture_4 = rg.Create(TextureDesc{
        .name   = "texture_4",
        .width  = 512,
        .height = 512,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::RenderTarget,
    });

    const auto compute_pass_1 =
        ComputePassBuilder(rg)
            .SetName("compute pass 1")
            .Write(buffer_1)
            .AddPipeline(compute_pipeline)
            .SetExecutor([](const RenderGraph& rg, const ComputePassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(compute_pass_1));

    const auto compute_pass_2 =
        ComputePassBuilder(rg)
            .SetName("compute pass 2")
            .Write(buffer_2)
            .AddPipeline(compute_pipeline)
            .SetExecutor([](const RenderGraph& rg, const ComputePassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(compute_pass_2));

    const auto render_pass_1 =
        RenderPassBuilder(rg)
            .SetName("render pass 1")
            .Read(buffer_1, PipelineStage::VertexShader)
            .Read(buffer_2, PipelineStage::PixelShader)
            .SetRenderTarget(texture_1)
            .AddPipeline(render_pipeline)
            .SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_1));

    const auto render_pass_2 =
        RenderPassBuilder(rg)
            .SetName("render pass 2")
            .Read(buffer_1, PipelineStage::VertexShader)
            .SetRenderTarget(texture_2)
            .AddPipeline(render_pipeline)
            .SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_2));

    const auto render_pass_3 =
        RenderPassBuilder(rg)
            .SetName("render pass 3")
            .Read(texture_1, {}, PipelineStage::PixelShader)
            .Read(texture_2, {}, PipelineStage::PixelShader)
            .SetRenderTarget(texture_3)
            .AddPipeline(render_pipeline)
            .SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_3));

    const auto unused_render_pass =
        RenderPassBuilder(rg)
            .SetName("unused render pass")
            .Read(texture_1, {}, PipelineStage::PixelShader)
            .Read(texture_2, {}, PipelineStage::PixelShader)
            .AddPipeline(render_pipeline)
            .SetRenderTarget(texture_4)
            .SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {})
            .Finish();
    ASSERT_TRUE(rg.IsValid(unused_render_pass));

    PresentPassBuilder(rg)
        .From(texture_3)
        .SetSwapChain(device->CreateSwapChain({}))
        .Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}