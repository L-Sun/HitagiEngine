#include "test_macros.hpp"
#include <spdlog/spdlog.h>

import std;
import gfx;
import asset;
import app;
import core;
import utils;
import math;
import test_utils;

using namespace hitagi::gfx;
using namespace hitagi::rg;
using namespace hitagi::math;
using namespace testing;

class RenderGraphTest : public Test {
protected:
    RenderGraphTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          device(create_device(Device::Type::DX12, test_name)),
          rg(*device, queues, *bindings, test_name) {
    }

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create device";
    }

    std::string             test_name;
    std::shared_ptr<Device> device;
    CommandQueues                  queues{*device};
    std::unique_ptr<BindlessUtils> bindings = BindlessUtils::Create(*device);
    ShaderCompiler                 compiler{"Tests"};
    RenderGraph             rg;
};

TEST_F(RenderGraphTest, ImportBuffer) {
    const auto buffer_0 = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                      .name   = std::pmr::string(std::format("Buffer-{}", test_name)),
                                                                      .size   = (sizeof(float)) * (16),
                                                                      .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                  });

    // import with empty name
    const auto buffer_handle_0 = rg.Import(buffer_0);
    EXPECT_TRUE(rg.IsValid(buffer_handle_0)) << "Import buffer with empty name should succeed";
    EXPECT_EQ(rg.Import(buffer_0, "new_name"), buffer_handle_0) << "Reimport buffer with new name should return same handle";

    const auto buffer_1 = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                      .name   = std::pmr::string(std::format("Buffer-{}", test_name)),
                                                                      .size   = (sizeof(float)) * (16),
                                                                      .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
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

        const auto diff_buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                             .name   = std::pmr::string(std::format("Buffer-{}", test_name)),
                                                                             .size   = (sizeof(float)) * (16),
                                                                             .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                         });
        EXPECT_FALSE(rg.IsValid(rg.Import(diff_buffer, buffer_1->GetName()))) << "Import buffer with existed name but different buffer should fail";
    }
}

TEST_F(RenderGraphTest, ImportTexture) {
    const auto texture_0 = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                        .name   = std::pmr::string(std::format("Texture-{}", test_name)),
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

    const auto texture_1 = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                        .name   = std::pmr::string(std::format("Texture-{}", test_name)),
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

        const auto diff_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                               .name   = std::pmr::string(std::format("Texture-{}", test_name)),
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
            .name   = std::pmr::string(std::format("Texture-{}", test_name)),
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
    const auto buffer_0 = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                      .name   = std::pmr::string(std::format("Buffer-{}", test_name)),
                                                                      .size   = (sizeof(float)) * (16),
                                                                      .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                  });

    const auto buffer_handle_0 = rg.Import(buffer_0);
    ASSERT_TRUE(rg.IsValid(buffer_handle_0));

    const auto buffer_handle_1 = rg.MoveFrom(buffer_handle_0);
    EXPECT_TRUE(rg.IsValid(buffer_handle_1)) << "Move buffer should succeed";
    EXPECT_NE(buffer_handle_0, buffer_handle_1) << "Move buffer should return different handle";
}

TEST_F(RenderGraphTest, MoveTexture) {
    const auto texture_0 = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                        .name   = std::pmr::string(std::format("Texture-{}", test_name)),
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

    auto app = hitagi::Application::CreateApp({
        .log_level = spdlog::level::to_string_view(spdlog::get_level()).data(),
        .headless  = true,
    });

    auto swap_chain = hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {
                                                                                                                  .window = app->GetWindow(),
                                                                                                              });

    const auto vertex_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                                  .name        = std::pmr::string(std::format("VS-{}", test_name)),
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

    const auto pixel_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                                 .name        = std::pmr::string(std::format("PS-{}", test_name)),
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

    const auto vertex_input_layout = compiler.ExtractVertexLayout(vertex_shader->GetDesc());
    const auto pipeline            = hitagi::gfx::RenderPipeline::Create(*device, *bindings,
                                                                         {
                                                                             .name                = std::pmr::string(std::format("Pipeline-{}", test_name)),
                                                                             .vertex_input_layout = vertex_input_layout,
                                                                         },
                                                                         {vertex_shader, pixel_shader});

    const auto output_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                             .name        = std::pmr::string(std::format("Texture-{}-{}", test_name, rg.GetFrameIndex())),
                                                                                             .width       = swap_chain->GetWidth(),
                                                                                             .height      = swap_chain->GetHeight(),
                                                                                             .format      = Format::R8G8B8A8_UNORM,
                                                                                             .clear_value = Color(0.0, 0.0, 0.0, 1.0),
                                                                                             .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
                                                                                         });
    ASSERT_TRUE(output_texture);

    RenderPassBuilder render_builder(rg);
    render_builder.SetName("RenderPass");
    const auto positions_access = render_builder.ReadAsVertices(rg.Create(
                                                                    {
                                                                        .name   = std::pmr::string(std::format("positions-{}", test_name)),
                                                                        .size   = (sizeof(vec3f)) * (3),
                                                                        .usages = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::MapWrite,
                                                                    },
                                                                    "positions"),
                                                                {.element_size = sizeof(vec3f), .element_count = 0});
    const auto colors_access    = render_builder.ReadAsVertices(rg.Create(
                                                                    {
                                                                        .name   = std::pmr::string(std::format("colors-{}", test_name)),
                                                                        .size   = (sizeof(vec3f)) * (3),
                                                                        .usages = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::MapWrite,
                                                                    },
                                                                    "colors"),
                                                                {.element_size = sizeof(vec3f), .element_count = 0});
    render_builder.SetRenderTarget(rg.Import(output_texture, "output"), true);
    render_builder.SetExecutor([=](const RenderGraph& rg, const RenderPassNode& pass) {
        auto rotate_matrix = rotate_z(deg2rad(static_cast<float>(rg.GetFrameIndex())));

        auto& position_buffer = pass.Resolve(rg.GetBufferHandle("positions"));
        auto  positions       = pass.Resolve(positions_access).GetMappedSpan<vec3f>();
        positions[0]          = (rotate_matrix * vec4f{0.0f, 0.5f, 0.0f, 1.0f}).xyz;
        positions[1]          = (rotate_matrix * vec4f{0.5f, -0.5f, 0.0f, 1.0f}).xyz;
        positions[2]          = (rotate_matrix * vec4f{-0.5f, -0.5f, 0.0f, 1.0f}).xyz;

        auto& color_buffer = pass.Resolve(rg.GetBufferHandle("colors"));
        auto  colors       = pass.Resolve(colors_access).GetMappedSpan<vec3f>();
        colors[0]          = {1.0f, 0.0f, 0.0f};
        colors[1]          = {0.0f, 1.0f, 0.0f};
        colors[2]          = {0.0f, 0.0f, 1.0f};

        auto& cmd = pass.GetCmd();
        cmd.SetPipeline(*pipeline);
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
    });
    const auto render_pass = render_builder.Finish();

    EXPECT_TRUE(render_pass);

    PresentPassBuilder present_builder(rg);
    present_builder.From(rg.GetTextureHandle("output"));
    present_builder.SetSwapChain(swap_chain);
    present_builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    swap_chain->Present();
    app->Tick();

    auto       pixels      = readback_texture(*device, queues, *bindings, *output_texture);
    const auto output_path = std::filesystem::path("temp") / "RenderGraphTest.AddRenderPass.png";
    std::filesystem::create_directories(output_path.parent_path());

    hitagi::asset::Texture image(
        output_texture->GetDesc().width,
        output_texture->GetDesc().height,
        output_texture->GetDesc().format,
        std::move(pixels));
    const auto png = hitagi::asset::PngEncoder{}.Encode(image);
    ASSERT_FALSE(png.Empty());
    hitagi::core::FileIOManager{}.SaveBuffer(png, output_path);
}

TEST_F(RenderGraphTest, GraphTest) {
    if (device->device_type != hitagi::gfx::Device::Type::Mock) {
        GTEST_SKIP();
    }

    const auto buffer_1 = rg.Create(GPUBufferDesc{
        .name   = "buffer_1",
        .size   = (sizeof(float)) * (1),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite | GPUBufferUsageFlags::MapWrite,
    });

    const auto buffer_2 = rg.Create(GPUBufferDesc{
        .name   = "buffer_2",
        .size   = (sizeof(float)) * (1),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite | GPUBufferUsageFlags::MapWrite,
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

    ComputePassBuilder buffer_1_builder(rg);
    buffer_1_builder.SetName("compute pass 1");
    buffer_1_builder.Write(buffer_1);
    buffer_1_builder.SetExecutor([](const RenderGraph& rg, const ComputePassNode& node) {});
    const auto compute_pass_1 = buffer_1_builder.Finish();
    ASSERT_TRUE(rg.IsValid(compute_pass_1));

    ComputePassBuilder buffer_2_builder(rg);
    buffer_2_builder.SetName("compute pass 2");
    buffer_2_builder.Write(buffer_2);
    buffer_2_builder.SetExecutor([](const RenderGraph& rg, const ComputePassNode& node) {});
    const auto compute_pass_2 = buffer_2_builder.Finish();
    ASSERT_TRUE(rg.IsValid(compute_pass_2));

    RenderPassBuilder texture_1_builder(rg);
    texture_1_builder.SetName("render pass 1");
    texture_1_builder.Read(buffer_1, {.element_count = 0}, PipelineStage::VertexShader);
    texture_1_builder.Read(buffer_2, {.element_count = 0}, PipelineStage::PixelShader);
    texture_1_builder.SetRenderTarget(texture_1);
    texture_1_builder.SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {});
    const auto render_pass_1 = texture_1_builder.Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_1));

    RenderPassBuilder texture_2_builder(rg);
    texture_2_builder.SetName("render pass 2");
    texture_2_builder.Read(buffer_1, {.element_count = 0}, PipelineStage::VertexShader);
    texture_2_builder.SetRenderTarget(texture_2);
    texture_2_builder.SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {});
    const auto render_pass_2 = texture_2_builder.Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_2));

    RenderPassBuilder composite_builder(rg);
    composite_builder.SetName("render pass 3");
    composite_builder.Read(texture_1, {}, PipelineStage::PixelShader);
    composite_builder.Read(texture_2, {}, PipelineStage::PixelShader);
    composite_builder.SetRenderTarget(texture_3);
    composite_builder.SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {});
    const auto render_pass_3 = composite_builder.Finish();
    ASSERT_TRUE(rg.IsValid(render_pass_3));

    RenderPassBuilder unused_builder(rg);
    unused_builder.SetName("unused render pass");
    unused_builder.Read(texture_1, {}, PipelineStage::PixelShader);
    unused_builder.Read(texture_2, {}, PipelineStage::PixelShader);
    unused_builder.SetRenderTarget(texture_4);
    unused_builder.SetExecutor([](const RenderGraph& rg, const RenderPassNode& node) {});
    const auto unused_render_pass = unused_builder.Finish();
    ASSERT_TRUE(rg.IsValid(unused_render_pass));

    PresentPassBuilder present_builder(rg);
    present_builder.From(texture_3);
    present_builder.SetSwapChain(hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {}));
    present_builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();
}

class RenderGraphCullingTest : public Test {
protected:
    RenderGraphCullingTest()
        : device(create_device(Device::Type::Mock, "RenderGraphCulling")),
          rg(*device, queues, *bindings, "RenderGraphCulling"),
          swap_chain(hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {})) {}

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create mock device";
    }

    auto CreateRenderTarget(std::string_view name) -> TextureHandle {
        return rg.Create(
            TextureDesc{
                .name   = std::pmr::string(name),
                .width  = 16,
                .height = 16,
                .depth  = 1,
                .format = Format::R8G8B8A8_UNORM,
                .usages = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
            },
            name);
    }

    std::shared_ptr<Device>    device;
    CommandQueues                  queues{*device};
    std::unique_ptr<BindlessUtils> bindings = BindlessUtils::Create(*device);
    ShaderCompiler                 compiler{"Tests"};
    RenderGraph                rg;
    std::shared_ptr<SwapChain> swap_chain;
};

TEST_F(RenderGraphCullingTest, CullsUnrootedPassWithoutPresent) {
    bool executed = false;

    RenderPassBuilder builder(rg);
    builder.SetName("unrooted_pass");
    builder.SetRenderTarget(CreateRenderTarget("unrooted_target"));
    builder.SetExecutor([&executed](const RenderGraph&, const RenderPassNode&) {
        executed = true;
    });
    builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    EXPECT_FALSE(executed);
}

TEST_F(RenderGraphCullingTest, ToDotSkipsFreedNodeSlotsAfterReset) {
    RenderPassBuilder builder(rg);
    builder.SetName("transient_pass");
    builder.SetRenderTarget(CreateRenderTarget("transient_target"));
    builder.SetExecutor([](const RenderGraph&, const RenderPassNode&) {});
    builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    const auto dot = rg.ToDot();
    EXPECT_NE(dot.find("digraph"), std::pmr::string::npos);
    EXPECT_EQ(dot.find("transient_pass"), std::pmr::string::npos);
}

TEST_F(RenderGraphCullingTest, NonCullablePassExecutesWithoutPresent) {
    bool executed = false;

    RenderPassBuilder builder(rg);
    builder.SetName("side_effect_pass");
    builder.AllowPassCulling(false);
    builder.SetRenderTarget(CreateRenderTarget("side_effect_target"));
    builder.SetExecutor([&executed](const RenderGraph&, const RenderPassNode&) {
        executed = true;
    });
    builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    EXPECT_TRUE(executed);
}

TEST_F(RenderGraphCullingTest, BufferExtractionKeepsProducerWithoutPresent) {
    bool producer_executed = false;

    const auto target = CreateRenderTarget("extracted_target");

    RenderPassBuilder builder(rg);
    builder.SetName("producer_pass");
    builder.SetRenderTarget(target);
    builder.SetExecutor([&producer_executed](const RenderGraph&, const RenderPassNode&) {
        producer_executed = true;
    });
    builder.Finish();

    const auto readback_buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                             .name   = "extraction_readback",
                                                                             .size   = (4) * (16 * 16),
                                                                             .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
                                                                         });

    const auto extraction = rg.QueueBufferExtraction(target, readback_buffer);
    ASSERT_TRUE(rg.IsValid(extraction));

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    EXPECT_TRUE(producer_executed);
}

TEST_F(RenderGraphCullingTest, CullsDisconnectedBranchWithPresent) {
    bool present_branch_executed = false;
    bool culled_branch_executed  = false;

    const auto present_target = CreateRenderTarget("present_target");

    RenderPassBuilder visible_builder(rg);
    visible_builder.SetName("present_branch");
    visible_builder.SetRenderTarget(present_target);
    visible_builder.SetExecutor([&present_branch_executed](const RenderGraph&, const RenderPassNode&) {
        present_branch_executed = true;
    });
    visible_builder.Finish();

    RenderPassBuilder culled_builder(rg);
    culled_builder.SetName("culled_branch");
    culled_builder.SetRenderTarget(CreateRenderTarget("culled_target"));
    culled_builder.SetExecutor([&culled_branch_executed](const RenderGraph&, const RenderPassNode&) {
        culled_branch_executed = true;
    });
    culled_builder.Finish();

    PresentPassBuilder present_builder(rg);
    present_builder.From(present_target);
    present_builder.SetSwapChain(swap_chain);
    present_builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    EXPECT_TRUE(present_branch_executed);
    EXPECT_FALSE(culled_branch_executed);
}

TEST_F(RenderGraphCullingTest, SideEffectPassesRespectLayerDependencies) {
    const auto producer_buffer    = rg.Create(GPUBufferDesc{
        .name   = "producer_buffer",
        .size   = (sizeof(float)) * (1),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
    });
    const auto independent_buffer = rg.Create(GPUBufferDesc{
        .name   = "independent_buffer",
        .size   = (sizeof(float)) * (1),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
    });

    std::mutex               execution_mutex;
    std::vector<std::string> execution_order;
    const auto               record_execution = [&](std::string name) {
        std::scoped_lock lock(execution_mutex);
        execution_order.emplace_back(std::move(name));
    };

    ComputePassBuilder producer_builder(rg);
    producer_builder.SetName("producer");
    producer_builder.Write(producer_buffer);
    producer_builder.SetExecutor([&](const RenderGraph&, const ComputePassNode&) {
        record_execution("producer");
    });
    const auto producer_pass = producer_builder.Finish();
    ASSERT_TRUE(rg.IsValid(producer_pass));

    ComputePassBuilder independent_builder(rg);
    independent_builder.SetName("independent");
    independent_builder.AllowPassCulling(false);
    independent_builder.Write(independent_buffer);
    independent_builder.SetExecutor([&](const RenderGraph&, const ComputePassNode&) {
        record_execution("independent");
    });
    const auto independent_pass = independent_builder.Finish();
    ASSERT_TRUE(rg.IsValid(independent_pass));

    ComputePassBuilder consumer_builder(rg);
    consumer_builder.SetName("consumer");
    consumer_builder.AllowPassCulling(false);
    consumer_builder.Read(producer_buffer);
    consumer_builder.SetExecutor([&](const RenderGraph&, const ComputePassNode&) {
        record_execution("consumer");
    });
    const auto consumer_pass = consumer_builder.Finish();
    ASSERT_TRUE(rg.IsValid(consumer_pass));

    EXPECT_TRUE(rg.Compile());
    rg.Execute();

    const auto producer_it    = std::ranges::find(execution_order, "producer");
    const auto independent_it = std::ranges::find(execution_order, "independent");
    const auto consumer_it    = std::ranges::find(execution_order, "consumer");

    EXPECT_EQ(execution_order.size(), 3);
    ASSERT_NE(producer_it, execution_order.end());
    ASSERT_NE(independent_it, execution_order.end());
    ASSERT_NE(consumer_it, execution_order.end());
    EXPECT_LT(std::ranges::distance(execution_order.begin(), producer_it), std::ranges::distance(execution_order.begin(), consumer_it));
}

TEST_F(RenderGraphTest, CopyTextureToBuffer) {
    if (device->device_type != hitagi::gfx::Device::Type::Mock) {
        GTEST_SKIP();
    }

    const auto src_texture = rg.Create(TextureDesc{
        .name   = "src_texture",
        .width  = 64,
        .height = 64,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::CopySrc,
    });

    const auto dst_buffer = rg.Create(GPUBufferDesc{
        .name   = "dst_buffer",
        .size   = (4) * (64 * 64),
        .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
    });

    CopyPassBuilder copy_builder(rg);
    copy_builder.SetName("texture_to_buffer_copy");
    copy_builder.TextureToBuffer(src_texture, dst_buffer);
    copy_builder.SetExecutor([=](const RenderGraph& rg, const CopyPassNode& pass) {
        auto& cmd     = pass.GetCmd();
        auto& texture = pass.Resolve(src_texture);
        auto& buffer  = pass.Resolve(dst_buffer);
        cmd.CopyTextureToBuffer(texture, {0, 0, 0}, {64, 64, 1}, buffer, 0);
    });
    const auto copy_pass = copy_builder.Finish();
    ASSERT_TRUE(rg.IsValid(copy_pass));

    PresentPassBuilder present_builder(rg);
    present_builder.From(src_texture);
    present_builder.SetSwapChain(hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {}));
    present_builder.Finish();

    EXPECT_TRUE(rg.Compile());
    rg.Execute();
}

TEST(RenderGraphAccessTest, RejectsConflictingAccessesAndPhysicalDescriptors) {
    auto        device = create_device(Device::Type::Mock, "GraphAccessValidation");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    RenderGraph                 graph(*device, queues, *bindings);
    const auto  buffer = graph.Create(GPUBufferDesc{.size = 32, .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite});
    {
        ComputePassBuilder builder(graph);
        EXPECT_TRUE(builder.Read(buffer));
        EXPECT_FALSE(builder.Write(buffer));
        EXPECT_FALSE(graph.IsValid(builder.Finish()));
    }
    {
        ComputePassBuilder builder(graph);
        auto               physical = hitagi::gfx::GPUBuffer::Create(*device, {.size = 32, .usages = GPUBufferUsageFlags::StorageRead});
        EXPECT_FALSE(builder.Read(buffer, {.buffer = physical}));
        EXPECT_FALSE(graph.IsValid(builder.Finish()));
    }
    const auto depth = graph.Create(TextureDesc{
        .width = 8, .height = 8, .depth = 1, .format = Format::D32_FLOAT, .usages = TextureUsageFlags::SRV | TextureUsageFlags::DepthStencil});
    {
        RenderPassBuilder builder(graph);
        EXPECT_TRUE(builder.Read(depth));
        EXPECT_FALSE(builder.ReadDepthStencil(depth));
        EXPECT_FALSE(graph.IsValid(builder.Finish()));
    }
}

TEST(RenderGraphAccessTest, MergesReadStagesPerResource) {
    auto              device = create_device(Device::Type::Mock, "GraphReadStages");
    hitagi::gfx::CommandQueues  queues(*device);
    auto                        bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler compiler{"Tests"};
    RenderGraph                 graph(*device, queues, *bindings);
    const auto        buffer = graph.Create(GPUBufferDesc{.size = 32, .usages = GPUBufferUsageFlags::StorageRead});
    const auto        target = graph.Create(TextureDesc{
        .width = 8, .height = 8, .depth = 1, .format = Format::R8G8B8A8_UNORM, .usages = TextureUsageFlags::RenderTarget});
    RenderPassBuilder builder(graph);
    builder.AllowPassCulling(false);
    builder.Read(buffer, {.element_size = 4, .element_count = 8}, PipelineStage::VertexShader);
    builder.Read(buffer, {.element_size = 8, .element_count = 4}, PipelineStage::PixelShader);
    builder.SetRenderTarget(target);
    bool executed = false;
    builder.SetExecutor([&](const RenderGraph&, const RenderPassNode& pass) {
        executed = true;
        // Mock-only: inspect tracked state while keeping the transition a no-op.
        const auto stages  = PipelineStage::VertexShader | PipelineStage::PixelShader;
        const auto barrier = pass.Resolve(buffer).Transition(BarrierAccess::ShaderRead, stages);
        EXPECT_EQ(barrier.src_stage, stages);
        EXPECT_EQ(barrier.src_access, BarrierAccess::ShaderRead);
        pass.GetCmd().ResourceBarrier({}, std::array{barrier});
    });
    ASSERT_TRUE(graph.IsValid(builder.Finish()));
    ASSERT_TRUE(graph.Compile());
    graph.Execute();
    EXPECT_TRUE(executed);
}

class TransientResourcePoolTest : public Test {
protected:
    TransientResourcePoolTest()
        : device(create_device(Device::Type::Mock, "TransientPool")),
          rg(*device, queues, *bindings, "TransientPool"),
          swap_chain(hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {})) {}

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create mock device";
    }

    struct FrameResources {
        GPUBuffer* buffer  = nullptr;
        Texture*   texture = nullptr;
    };

    auto RunFrame(const GPUBufferDesc& buffer_desc, const TextureDesc& texture_desc) -> FrameResources {
        FrameResources result;

        const auto buffer  = rg.Create(buffer_desc, "buf");
        const auto texture = rg.Create(texture_desc, "tex");

        RenderPassBuilder render_builder(rg);
        render_builder.SetName("pass");
        render_builder.Read(buffer, {.element_count = 0}, PipelineStage::VertexShader);
        render_builder.SetRenderTarget(texture);
        render_builder.SetExecutor([&result, buffer, texture](const RenderGraph& rg_ref, const RenderPassNode& node) {
            result.buffer  = &node.Resolve(buffer);
            result.texture = &node.Resolve(texture);
        });
        render_builder.Finish();

        PresentPassBuilder present_builder(rg);
        present_builder.From(texture);
        present_builder.SetSwapChain(swap_chain);
        present_builder.Finish();

        EXPECT_TRUE(rg.Compile());
        rg.Execute();
        return result;
    }

    std::shared_ptr<Device>    device;
    CommandQueues                  queues{*device};
    std::unique_ptr<BindlessUtils> bindings = BindlessUtils::Create(*device);
    ShaderCompiler                 compiler{"Tests"};
    RenderGraph                rg;
    std::shared_ptr<SwapChain> swap_chain;
};

TEST_F(TransientResourcePoolTest, BufferAndTextureReuse) {
    const GPUBufferDesc buffer_desc{
        .name   = "test_buffer",
        .size   = (sizeof(float)) * (16),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
    };
    const TextureDesc texture_desc{
        .name   = "test_texture",
        .width  = 512,
        .height = 512,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    };

    auto frame1 = RunFrame(buffer_desc, texture_desc);
    ASSERT_NE(frame1.buffer, nullptr);
    ASSERT_NE(frame1.texture, nullptr);

    auto frame2 = RunFrame(buffer_desc, texture_desc);
    ASSERT_NE(frame2.buffer, nullptr);
    ASSERT_NE(frame2.texture, nullptr);

    EXPECT_EQ(frame1.buffer, frame2.buffer) << "Buffer should be reused from pool";
    EXPECT_EQ(frame1.texture, frame2.texture) << "Texture should be reused from pool";
}

TEST_F(TransientResourcePoolTest, NoReuseOnDescMismatch) {
    const GPUBufferDesc buffer_desc_a{
        .name   = "buffer_a",
        .size   = (sizeof(float)) * (16),
        .usages = GPUBufferUsageFlags::StorageRead,
    };
    const TextureDesc texture_desc_a{
        .name   = "texture_a",
        .width  = 512,
        .height = 512,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    };

    auto frame1 = RunFrame(buffer_desc_a, texture_desc_a);

    const GPUBufferDesc buffer_desc_b{
        .name   = "buffer_b",
        .size   = (sizeof(float)) * (32),
        .usages = GPUBufferUsageFlags::StorageRead,
    };
    const TextureDesc texture_desc_b{
        .name   = "texture_b",
        .width  = 1024,
        .height = 1024,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    };

    auto frame2 = RunFrame(buffer_desc_b, texture_desc_b);

    EXPECT_NE(frame1.buffer, frame2.buffer) << "Different desc should not reuse buffer";
    EXPECT_NE(frame1.texture, frame2.texture) << "Different desc should not reuse texture";
}

TEST_F(TransientResourcePoolTest, NameIndependentReuse) {
    const GPUBufferDesc buffer_desc_frame1{
        .name   = "buffer_frame1",
        .size   = (sizeof(float)) * (16),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
    };
    const TextureDesc texture_desc_frame1{
        .name   = "texture_frame1",
        .width  = 256,
        .height = 256,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    };

    auto frame1 = RunFrame(buffer_desc_frame1, texture_desc_frame1);

    const GPUBufferDesc buffer_desc_frame2{
        .name   = "buffer_frame2_different_name",
        .size   = (sizeof(float)) * (16),
        .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
    };
    const TextureDesc texture_desc_frame2{
        .name   = "texture_frame2_different_name",
        .width  = 256,
        .height = 256,
        .depth  = 1,
        .format = Format::R8G8B8A8_UNORM,
        .usages = TextureUsageFlags::SRV | TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
    };

    auto frame2 = RunFrame(buffer_desc_frame2, texture_desc_frame2);

    EXPECT_EQ(frame1.buffer, frame2.buffer) << "Pool should match by structure, not name";
    EXPECT_EQ(frame1.texture, frame2.texture) << "Pool should match by structure, not name";
}
