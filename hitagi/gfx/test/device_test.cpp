#include <hitagi/core/buffer.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::core;
using namespace hitagi::gfx;
using namespace hitagi::math;

TEST(CreateDeviceTest, CreateD3D) {
    EXPECT_NO_THROW({
        auto device = Device::Create(Device::Type::DX12, ::testing::UnitTest::GetInstance()->current_test_info()->name());
    });
}

class D3DDeviceTest : public ::testing::Test {
public:
    D3DDeviceTest() : device(Device::Create(Device::Type::DX12, ::testing::UnitTest::GetInstance()->current_test_info()->name())) {}
    std::unique_ptr<Device> device;
};

TEST_F(D3DDeviceTest, CreateCommandQueue) {
    {
        auto graphics_queue = device->CreateCommandQueue(CommandType::Graphics);
        ASSERT_TRUE(graphics_queue);
        EXPECT_EQ(graphics_queue->type, CommandType::Graphics);
    }
    {
        auto compute_queue = device->CreateCommandQueue(CommandType::Compute);
        ASSERT_TRUE(compute_queue);
        EXPECT_EQ(compute_queue->type, CommandType::Compute);
    }
    {
        auto copy_queue = device->CreateCommandQueue(CommandType::Copy);
        ASSERT_TRUE(copy_queue);
        EXPECT_EQ(copy_queue->type, CommandType::Copy);
    }
}

TEST_F(D3DDeviceTest, CreateGpuBuffer) {
    // Create read back buffer
    {
        auto gpu_buffer = device->CreateBuffer(
            {
                .name   = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size   = 1024_kB,
                .usages = GpuBuffer::UsageFlags::MapRead | GpuBuffer::UsageFlags::CopyDst,
            });

        ASSERT_TRUE(gpu_buffer != nullptr);
        EXPECT_TRUE(gpu_buffer->mapped_ptr != nullptr);
        EXPECT_EQ(gpu_buffer->desc.size, 1024_kB);
    }

    // Create upload buffer
    {
        auto gpu_buffer = device->CreateBuffer(
            {
                .name   = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size   = 1024_kB,
                .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            });

        ASSERT_TRUE(gpu_buffer != nullptr);
        EXPECT_TRUE(gpu_buffer->mapped_ptr != nullptr);
        EXPECT_EQ(gpu_buffer->desc.size, 1024_kB);

        Buffer cpu_buffer(1024_kB);
        std::memcpy(gpu_buffer->mapped_ptr, cpu_buffer.GetData(), 1024_kB);
    }

    // Create default buffer
    {
        auto gpu_buffer = device->CreateBuffer(
            {
                .name = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size = 1024_kB,
            });

        ASSERT_TRUE(gpu_buffer != nullptr);
        EXPECT_TRUE(gpu_buffer->mapped_ptr == nullptr);
        EXPECT_EQ(gpu_buffer->desc.size, 1024_kB);
    }

    // Create upload buffer with initial data
    {
        constexpr std::string_view initial_data = "abcdefg";

        auto gpu_buffer = device->CreateBuffer(
            {
                .name   = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size   = initial_data.size(),
                .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            },
            {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});

        ASSERT_TRUE(gpu_buffer != nullptr);
        EXPECT_TRUE(gpu_buffer->mapped_ptr != nullptr);
        EXPECT_EQ(gpu_buffer->desc.size, initial_data.size());
        EXPECT_STREQ(initial_data.data(), reinterpret_cast<const char*>(gpu_buffer->mapped_ptr));
    }

    // Create default buffer with initial data
    {
        std::string initial_data = "abcdefg";

        auto gpu_buffer = device->CreateBuffer(
            {
                .name = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size = initial_data.size(),
            },
            {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});

        ASSERT_TRUE(gpu_buffer != nullptr);
        EXPECT_TRUE(gpu_buffer->mapped_ptr == nullptr);
        EXPECT_EQ(gpu_buffer->desc.size, initial_data.size());
    }
}

TEST_F(D3DDeviceTest, CreateTexture) {
    // Create 1D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });
        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->desc.width, 128);
        EXPECT_EQ(texture->desc.height, 1);
        EXPECT_EQ(texture->desc.depth, 1);
    }
    // Create 2D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            },
            {});

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->desc.width, 128);
        EXPECT_EQ(texture->desc.height, 128);
        EXPECT_EQ(texture->desc.depth, 1);
    }
    // Create 3D texture
    {
        auto texture = device->CreateTexture(
            {
                .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .depth       = 128,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->desc.width, 128);
        EXPECT_EQ(texture->desc.height, 128);
        EXPECT_EQ(texture->desc.depth, 128);
    }
    // Create 2D texture array
    {
        auto texture = device->CreateTexture(
            {
                .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .width       = 128,
                .height      = 128,
                .array_size  = 6,
                .format      = Format::R8G8B8A8_UNORM,
                .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
            });

        ASSERT_TRUE(texture != nullptr);
        EXPECT_EQ(texture->desc.width, 128);
        EXPECT_EQ(texture->desc.height, 128);
    }
}

TEST_F(D3DDeviceTest, CreateTextureView) {
    auto texture = device->CreateTexture(
        {
            .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            .width       = 128,
            .height      = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        });
    auto texture_view = device->CreateTextureView(
        {
            .textuer           = texture,
            .format            = texture->desc.format,
            .is_cube           = true,
            .mip_level_count   = texture->desc.mip_levels,
            .array_layer_count = texture->desc.array_size,
        });
    EXPECT_TRUE(texture_view);
}

TEST_F(D3DDeviceTest, CreateSampler) {
    auto sampler = device->CreatSampler(

        {
            .name      = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            .address_u = Sampler::AddressMode::Repeat,
            .address_v = Sampler::AddressMode::Repeat,
            .address_w = Sampler::AddressMode::Repeat,
            .compare   = CompareFunction::Always,
        });

    ASSERT_TRUE(sampler != nullptr);
}

TEST_F(D3DDeviceTest, CompileShader) {
    // Test Vertex Shader
    {
        auto vs_shader = device->CompileShader(
            {
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
            });

        ASSERT_TRUE(vs_shader != nullptr);
        EXPECT_FALSE(vs_shader->binary_data.Empty());
    }
    {
        auto ps_shader = device->CompileShader(
            {
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
            });
        ASSERT_TRUE(ps_shader != nullptr);
        EXPECT_FALSE(ps_shader->binary_data.Empty());
    }
}

TEST_F(D3DDeviceTest, CreatePipeline) {
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
                .name = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .vs   = device->CompileShader(
                      {
                          .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                          .type        = Shader::Type::Vertex,
                          .entry       = "VSMain",
                          .source_code = vs_code,
                    }),
                .input_layout = {{"POSITION", 0, 0, 0, Format::R32G32B32_FLOAT, false, 0}},

            });
        EXPECT_TRUE(render_pipeline);
    }
}

TEST_F(D3DDeviceTest, CommandContextTest) {
    // Graphics context test
    {
        constexpr auto vs_code = R"""(
            #define RSDEF                                    \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
            "RootConstants(num32BitConstants=1, b0)"                                        \

            cbuffer ConstantData : register(b0) {
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
                output.pos = float4(input.pos, 1.0f);
                return output;
            }
        )""";

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .vs   = device->CompileShader(
                      {
                          .name        = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                          .type        = Shader::Type::Vertex,
                          .entry       = "VSMain",
                          .source_code = vs_code,
                    }),
                .input_layout = {{"POSITION", 0, 0, 0, Format::R32G32B32_FLOAT, false, 0}},
            });
        ASSERT_TRUE(render_pipeline);

        auto constant_buffer = device->CreateBuffer(
            {
                .name   = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .size   = sizeof(vec4f),
                .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::CopySrc,
            });
        auto constant_buffer_span = std::span<vec4f>{
            reinterpret_cast<vec4f*>(constant_buffer->mapped_ptr),
            constant_buffer->desc.size / sizeof(vec4f)};
        constant_buffer_span[0] = vec4f(1, 2, 3, 4);

        auto graphics_context = device->CreateGraphicsContext(::testing::UnitTest::GetInstance()->current_test_info()->name());
        ASSERT_TRUE(graphics_context);
        EXPECT_NO_THROW({
            graphics_context->SetPipeline(*render_pipeline);
            graphics_context->PushConstant(0, *constant_buffer);
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

        auto upload_buffer = device->CreateBuffer(
            {
                .name   = fmt::format("Upload-{}", ::testing::UnitTest::GetInstance()->current_test_info()->name()),
                .size   = initial_data.size(),
                .usages = GpuBuffer::UsageFlags::CopySrc | GpuBuffer::UsageFlags::MapWrite,
            },
            {reinterpret_cast<const std::byte*>(initial_data.data()), initial_data.size()});
        auto readback_buffer = device->CreateBuffer(
            {
                .name   = fmt::format("Upload-{}", ::testing::UnitTest::GetInstance()->current_test_info()->name()),
                .size   = initial_data.size(),
                .usages = GpuBuffer::UsageFlags::CopyDst | GpuBuffer::UsageFlags::MapRead,
            });

        copy_context->CopyBuffer(*upload_buffer, 0, *readback_buffer, 0, initial_data.size());
        copy_context->End();

        auto copy_queue  = device->GetCommandQueue(CommandType::Copy);
        auto fence_value = copy_queue->Submit({copy_context.get()});
        copy_queue->WaitForFence(fence_value);

        EXPECT_STREQ(reinterpret_cast<const char*>(readback_buffer->mapped_ptr), initial_data.data());
    }
}

TEST_F(D3DDeviceTest, SwapChainTest) {
    auto app = hitagi::Application::CreateApp();
    app->Initialize();
    {
        auto rect = app->GetWindowsRect();

        auto swap_chain = device->CreateSwapChain(
            {
                .name       = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .window_ptr = app->GetWindow(),
                .format     = Format::R8G8B8A8_UNORM,
            });
        ASSERT_TRUE(swap_chain);

        auto back_buffer = swap_chain->GetBuffer(0);
        ASSERT_TRUE(back_buffer);
        EXPECT_EQ(back_buffer->desc.width, rect.right - rect.left);
        EXPECT_EQ(back_buffer->desc.height, rect.bottom - rect.top);
        EXPECT_EQ(back_buffer->desc.format, swap_chain->desc.format);
        EXPECT_TRUE(hitagi::utils::has_flag(back_buffer->desc.usages, Texture::UsageFlags::RTV));

        auto same_swapchan = device->CreateSwapChain(
            {
                .name       = "Same-Swaichain",
                .window_ptr = app->GetWindow(),
                .format     = Format::R8G8B8A8_UNORM,
            });
        EXPECT_EQ(same_swapchan, swap_chain);
    }
    app->Finalize();
}

TEST_F(D3DDeviceTest, IKownDirectX12) {
    auto app = hitagi::Application::CreateApp();
    app->Initialize();
    {
        auto rect = app->GetWindowsRect();

        auto swap_chain = device->CreateSwapChain(
            {
                .name       = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
                .window_ptr = app->GetWindow(),
                .format     = Format::R8G8B8A8_UNORM,
            });

        constexpr std::string_view shader_code = R"""(
            #define RSDEF                                    \
            "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
            "DescriptorTable(CBV(b0))"                       \

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
                {"COLOR", 0, 1, 0, Format::R32G32B32_FLOAT},
            },
            .rasterizer_config = {
                .front_counter_clockwise = false,
            },
        });

        float aspect = static_cast<float>(rect.right - rect.left) / static_cast<float>(rect.bottom - rect.top);

        std::array triangle_positions = {
            vec3f(0, 0.5 * aspect, 0),
            vec3f(0.5, -0.5 * aspect, 0),
            vec3f(-0.5, -0.5 * aspect, 0),
        };
        constexpr std::array triangle_colors = {
            vec3f(1, 0, 0),
            vec3f(0, 1, 0),
            vec3f(0, 0, 1),
        };
        auto positions_buffer = device->CreateBufferView(
            {
                .buffer = device->CreateBuffer(
                    {
                        .name = "I know DirectX12 positions buffer",
                        .size = triangle_positions.size() * sizeof(vec3f),
                    },
                    {reinterpret_cast<const std::byte*>(triangle_positions.data()), triangle_positions.size() * sizeof(vec3f)}),
                .size   = triangle_positions.size() * sizeof(vec3f),
                .stride = sizeof(vec3f),
                .usages = GpuBufferView::UsageFlags::Vertex,
            });

        auto colors_buffer = device->CreateBufferView(
            {
                .buffer = device->CreateBuffer(
                    {
                        .name = "I know DirectX12 color buffer",
                        .size = triangle_colors.size() * sizeof(vec3f),
                    },
                    {reinterpret_cast<const std::byte*>(triangle_colors.data()), triangle_colors.size() * sizeof(vec3f)}),
                .size   = triangle_colors.size() * sizeof(vec3f),
                .stride = sizeof(vec3f),
                .usages = GpuBufferView::UsageFlags::Vertex,
            });
        auto constant_buffer = device->CreateBufferView(
            {
                .buffer = device->CreateBuffer(
                    {
                        .name   = "Rotation",
                        .size   = sizeof(mat4f),
                        .usages = GpuBuffer::UsageFlags::MapWrite | GpuBuffer::UsageFlags::Constant,
                    }),
                .size   = sizeof(mat4f),
                .stride = sizeof(mat4f),
                .usages = GpuBufferView::UsageFlags::Constant,
            });

        auto& rotation = *reinterpret_cast<mat4f*>(constant_buffer->desc.buffer->mapped_ptr);

        auto render_queue = device->GetCommandQueue(CommandType::Graphics);
        auto context      = device->CreateGraphicsContext("I know DirectX12 context");
        auto rtv          = device->CreateTextureView(
                     {
                         .textuer = swap_chain->GetBuffer(swap_chain->GetCurrentBackIndex()),
                         .format  = swap_chain->desc.format,
            });

        context->SetRenderTarget(*rtv);
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
        context->ClearRenderTarget(*rtv);
        context->SetPipeline(*pipeline);
        context->SetVertexBuffer(0, *positions_buffer);
        context->SetVertexBuffer(1, *colors_buffer);

        rotation = rotate_z(deg2rad(90.0f));
        context->BindConstantBuffer(0, *constant_buffer);

        context->Draw(3);
        context->Present(*rtv);

        context->End();

        render_queue->Submit({context.get()});
        swap_chain->Present();
    }

    app->Finalize();
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}