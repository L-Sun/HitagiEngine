#include "test_macros.hpp"
#include <cstdint>
import interop.gtest;
import interop.spdlog;
import interop.magic_enum;

import std;
import gfx;
import asset;
import app;
import core;
import math;
import utils;
import test_utils;

using namespace hitagi::core;
using namespace hitagi::gfx;
using namespace hitagi::math;
using namespace hitagi::utils;

using namespace testing;

using namespace std::literals;

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

namespace {
auto CreateBufferView(Device& device, BindlessUtils& bindings, const std::shared_ptr<GPUBuffer>& buffer, std::uint64_t element_size) -> std::shared_ptr<GPUBufferView> {
    return hitagi::gfx::GPUBufferView::Create(device, bindings, GPUBufferViewDesc{
                                                                    .name          = std::pmr::string(buffer->GetName()),
                                                                    .buffer        = buffer,
                                                                    .type          = GPUBufferViewType::StorageRead,
                                                                    .offset        = 0,
                                                                    .element_size  = element_size,
                                                                    .element_count = 1,
                                                                });
}
}  // namespace

constexpr std::array supported_device_types = {
#ifdef _WIN32
    Device::Type::DX12,
#endif
    Device::Type::Vulkan,
};
class DeviceTest : public TestWithParam<Device::Type> {
protected:
    DeviceTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          device(create_device(GetParam(), test_name)) {}

    std::string             test_name;
    std::unique_ptr<Device> device;
    hitagi::gfx::CommandQueues                  queues{*device};
    std::unique_ptr<hitagi::gfx::BindlessUtils> bindings = hitagi::gfx::BindlessUtils::Create(*device);
    hitagi::gfx::ShaderCompiler                 compiler{"Tests"};
};
INSTANTIATE_TEST_SUITE_P(
    DeviceTest,
    DeviceTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(DeviceTest, CreateDevice) {
    EXPECT_TRUE(device != nullptr);
}

class GPUBufferTest : public DeviceTest {};
INSTANTIATE_TEST_SUITE_P(
    GPUBufferTest,
    GPUBufferTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(GPUBufferTest, Create) {
    EXPECT_THROW(hitagi::gfx::GPUBuffer::Create(*device, {
                                                             .name = std::pmr::string(test_name),
                                                             .size = (0) * (1),
                                                         }),
                 std::invalid_argument)
        << "should throw exception when element_size is 0";

    EXPECT_THROW(hitagi::gfx::GPUBuffer::Create(*device, {
                                                             .name = std::pmr::string(test_name),
                                                             .size = (1) * (0),
                                                         }),
                 std::invalid_argument)
        << "should throw exception when element_count is 0";

    EXPECT_TRUE(hitagi::gfx::GPUBuffer::Create(*device, {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = (sizeof(vec3f)) * (1024),
                                                            .usages = GPUBufferUsageFlags::Vertex,
                                                        }))
        << "should create vertex buffer successfully";

    EXPECT_TRUE(hitagi::gfx::GPUBuffer::Create(*device, {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = (sizeof(std::uint32_t)) * (1024),
                                                            .usages = GPUBufferUsageFlags::Index,
                                                        }))
        << "should create index buffer successfully";

    EXPECT_TRUE(hitagi::gfx::GPUBuffer::Create(*device, {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = (sizeof(std::uint16_t)) * (1024),
                                                            .usages = GPUBufferUsageFlags::Index,
                                                        }))
        << "should create index buffer successfully";

    EXPECT_TRUE(hitagi::gfx::GPUBuffer::Create(*device, {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = sizeof(std::uint8_t) * 1024,
                                                            .usages = GPUBufferUsageFlags::Index,
                                                        }))
        << "GPUBuffer is raw storage; index format is provided by SetIndexBuffer.";

    auto constant_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                          {
                                                              .name   = std::pmr::string(test_name),
                                                              .size   = sizeof(mat4f) * 1024,
                                                              .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
                                                          });
    EXPECT_TRUE(constant_buffer) << "should create constant buffer successfully.";
    EXPECT_EQ(constant_buffer->Size(), sizeof(mat4f) * 1024);

    EXPECT_TRUE(hitagi::gfx::GPUBuffer::Create(*device, {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = (sizeof(vec4f)) * (1024),
                                                            .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
                                                        }))
        << "should create storage buffer successfully.";
}

TEST_P(GPUBufferTest, Mapping) {
    auto mapped_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                        {
                                                            .name   = std::pmr::string(test_name),
                                                            .size   = (sizeof(vec3f)) * (1024),
                                                            .usages = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
                                                        });
    ASSERT_TRUE(mapped_buffer);
    EXPECT_TRUE(mapped_buffer->Map()) << "should map successfully.";
    EXPECT_NO_THROW(mapped_buffer->UnMap()) << "should unmap successfully.";

    auto no_mapped_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                           {
                                                               .name   = std::pmr::string(test_name),
                                                               .size   = (sizeof(vec3f)) * (1024),
                                                               .usages = GPUBufferUsageFlags::CopySrc,
                                                           });
    ASSERT_TRUE(no_mapped_buffer);
    EXPECT_THROW(static_cast<void>(no_mapped_buffer->Map()), std::runtime_error)
        << "can not map buffer without usage"
        << magic_enum::enum_flags_name(GPUBufferUsageFlags::MapWrite)
        << "or"
        << magic_enum::enum_flags_name(GPUBufferUsageFlags::MapRead);
    EXPECT_THROW(no_mapped_buffer->UnMap(), std::runtime_error) << "can not unmap buffer without mapping.";
}

TEST_P(GPUBufferTest, CreateBufferView) {
    static_assert(!std::is_constructible_v<GPUBufferView::MappedSpan<vec3f>, GPUBuffer&>);
    static_assert(!std::is_constructible_v<GPUBufferView::MappedSpan<vec3f>, GPUBufferView&>);
    static_assert(!std::is_constructible_v<GPUBufferView::MappedSpan<const vec3f>, GPUBufferView&>);
    static_assert(std::is_same_v<decltype(std::declval<GPUBufferView&>().GetMappedSpan<vec3f>()), GPUBufferView::MappedSpan<vec3f>>);
    static_assert(std::is_same_v<decltype(std::declval<const GPUBufferView&>().GetMappedSpan<vec3f>()), GPUBufferView::MappedSpan<const vec3f>>);
    auto buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                 {
                                                     .name   = std::pmr::string(test_name),
                                                     .size   = (sizeof(vec3f)) * (1024),
                                                     .usages = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
                                                 });
    ASSERT_TRUE(buffer);
    auto view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .element_size = sizeof(vec3f), .element_count = 0});
    EXPECT_NO_THROW(auto mapped = view->GetMappedSpan<vec3f>());
    EXPECT_NO_THROW(auto mapped = view->GetMappedSpan<const vec3f>());
    EXPECT_NO_THROW(auto mapped = std::as_const(*view).GetMappedSpan<vec3f>());

    auto no_map_written_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                                {
                                                                    .name   = std::pmr::string(test_name),
                                                                    .size   = (sizeof(vec3f)) * (1024),
                                                                    .usages = GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst,
                                                                });
    ASSERT_TRUE(no_map_written_buffer);
    auto read_only_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = no_map_written_buffer, .element_size = sizeof(vec3f), .element_count = 0});
    EXPECT_NO_THROW(auto mapped = read_only_view->GetMappedSpan<const vec3f>());
    EXPECT_THROW(auto mapped = read_only_view->GetMappedSpan<vec3f>(), std::invalid_argument)
        << "can not create not constant buffer view from buffer without flag "
        << flags_name(GPUBufferUsageFlags::MapWrite);

    auto no_mapped_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                           {
                                                               .name   = std::pmr::string(test_name),
                                                               .size   = (sizeof(vec3f)) * (1024),
                                                               .usages = GPUBufferUsageFlags::CopySrc,
                                                           });
    ASSERT_TRUE(no_mapped_buffer);
    auto unmappable_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = no_mapped_buffer, .element_size = sizeof(vec3f), .element_count = 0});
    EXPECT_THROW(auto mapped = unmappable_view->GetMappedSpan<vec3f>(), std::invalid_argument)
        << "can not create buffer view from buffer without flag "
        << flags_name(GPUBufferUsageFlags::MapRead)
        << " or "
        << flags_name(GPUBufferUsageFlags::MapWrite);
}

TEST_P(GPUBufferTest, CreateBufferWithInitialData) {
    std::vector<int> data(1024);
    std::iota(data.begin(), data.end(), 0);
    std::span<std::byte> data_span = {reinterpret_cast<std::byte*>(data.data()), data.size() * sizeof(int)};

    GPUBufferDesc desc{
        .name   = std::pmr::string(test_name),
        .size   = (sizeof(int)) * (1024),
        .usages = GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
    };

    auto buffer = hitagi::gfx::GPUBuffer::Create(*device, desc, data_span);
    ASSERT_TRUE(buffer);
    auto view        = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = buffer, .element_size = sizeof(int), .element_count = 0});
    auto buffer_view = view->GetMappedSpan<const int>();
    for (auto i = 0; i < 1024; ++i) {
        EXPECT_EQ(buffer_view[i], i) << "buffer data not match at index " << i;
    }

    desc.usages = GPUBufferUsageFlags::CopySrc;
    EXPECT_THROW(hitagi::gfx::GPUBuffer::Create(*device, desc, data_span), std::invalid_argument)
        << "can not create buffer with initial data without flag "
        << flags_name(GPUBufferUsageFlags::CopyDst);
}

TEST_P(DeviceTest, CreateGraphicsCommandContext) {
    auto context = hitagi::gfx::GraphicsCommandContext::Create(*device, queues, *bindings);
    EXPECT_TRUE(context != nullptr) << "Failed to create graphics context";
}

TEST_P(DeviceTest, CreateComputeCommandContext) {
    auto context = hitagi::gfx::ComputeCommandContext::Create(*device, queues, *bindings);
    EXPECT_TRUE(context != nullptr) << "Failed to create compute context";
}

TEST_P(DeviceTest, CreateCopyCommandContext) {
    auto context = hitagi::gfx::CopyCommandContext::Create(*device, queues);
    EXPECT_TRUE(context != nullptr) << "Failed to create copy context";
}

TEST_P(DeviceTest, CreateTexture1D) {
    auto texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                {
                                                    .name        = std::pmr::string(test_name),
                                                    .width       = 128,
                                                    .format      = Format::R8G8B8A8_UNORM,
                                                    .clear_value = Color::White(),
                                                });
    EXPECT_TRUE(texture != nullptr);
}

TEST_P(DeviceTest, CreateTexture2D) {
    Buffer data(128 * 128 * sizeof(vec4f));

    auto texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                {
                                                    .name        = std::pmr::string(test_name),
                                                    .width       = 128,
                                                    .height      = 128,
                                                    .format      = Format::R8G8B8A8_UNORM,
                                                    .clear_value = Color::White(),
                                                    .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
                                                },
                                                data.Span<const std::byte>());

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(DeviceTest, CreateTextureView) {
    auto srv_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                    .name   = std::pmr::string(std::format("{}-srv", test_name)),
                                                                                    .width  = 128,
                                                                                    .height = 128,
                                                                                    .format = Format::R8G8B8A8_UNORM,
                                                                                    .usages = TextureUsageFlags::SRV,
                                                                                });
    ASSERT_TRUE(srv_texture != nullptr);

    auto srv_view = hitagi::gfx::TextureView::Create(*device, *bindings, {
                                                                             .name    = std::pmr::string(std::format("{}-srv-view", test_name)),
                                                                             .texture = srv_texture,
                                                                             .type    = TextureViewType::ShaderRead,
                                                                         });
    ASSERT_TRUE(srv_view != nullptr);
    EXPECT_TRUE(srv_view->GetBindlessHandle());
    EXPECT_EQ(srv_view->GetBindlessHandle().type, BindlessHandleType::Texture);
    EXPECT_EQ(srv_view->GetBindlessHandle().writable, 0);

    auto uav_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                    .name   = std::pmr::string(std::format("{}-uav", test_name)),
                                                                                    .width  = 128,
                                                                                    .height = 128,
                                                                                    .format = Format::R32_UINT,
                                                                                    .usages = TextureUsageFlags::UAV,
                                                                                });
    ASSERT_TRUE(uav_texture != nullptr);

    auto uav_view = hitagi::gfx::TextureView::Create(*device, *bindings, {
                                                                             .name    = std::pmr::string(std::format("{}-uav-view", test_name)),
                                                                             .texture = uav_texture,
                                                                             .type    = TextureViewType::ShaderWrite,
                                                                         });
    ASSERT_TRUE(uav_view != nullptr);
    EXPECT_TRUE(uav_view->GetBindlessHandle());
    EXPECT_EQ(uav_view->GetBindlessHandle().type, BindlessHandleType::Texture);
    EXPECT_EQ(uav_view->GetBindlessHandle().writable, 1);

    auto copy_dst_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                         .name   = std::pmr::string(std::format("{}-copy-dst", test_name)),
                                                                                         .width  = 128,
                                                                                         .height = 128,
                                                                                         .format = Format::R8G8B8A8_UNORM,
                                                                                         .usages = TextureUsageFlags::CopyDst,
                                                                                     });
    ASSERT_TRUE(copy_dst_texture != nullptr);

    EXPECT_THROW(
        hitagi::gfx::TextureView::Create(*device, *bindings, {
                                                                 .name    = std::pmr::string(std::format("{}-invalid-bindless-view", test_name)),
                                                                 .texture = copy_dst_texture,
                                                                 .type    = TextureViewType::ShaderRead,
                                                             }),
        std::invalid_argument);
}

TEST_P(DeviceTest, CreateTexture3D) {
    auto texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                {
                                                    .name        = std::pmr::string(test_name),
                                                    .width       = 128,
                                                    .height      = 128,
                                                    .depth       = 128,
                                                    .format      = Format::R8G8B8A8_UNORM,
                                                    .clear_value = Color::White(),
                                                });

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(DeviceTest, CreateTexture2DArray) {
    auto texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                {
                                                    .name        = std::pmr::string(test_name),
                                                    .width       = 128,
                                                    .height      = 128,
                                                    .array_size  = 6,
                                                    .format      = Format::R8G8B8A8_UNORM,
                                                    .clear_value = Color::White(),
                                                });

    EXPECT_TRUE(texture != nullptr);
}

TEST_P(DeviceTest, CreateSampler) {
    auto sampler = hitagi::gfx::Sampler::Create(*device, *bindings,
                                                {
                                                    .name       = std::pmr::string(test_name),
                                                    .address_u  = AddressMode::Repeat,
                                                    .address_v  = AddressMode::Repeat,
                                                    .address_w  = AddressMode::Repeat,
                                                    .compare_op = CompareOp::Always,
                                                });

    ASSERT_TRUE(sampler != nullptr);
}

TEST_P(DeviceTest, CreateShader) {
    const std::pmr::string shader_code = R"""(
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
        auto vs_shader = hitagi::gfx::Shader::Create(*device, compiler, {
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
        auto ps_shader = hitagi::gfx::Shader::Create(*device, compiler, {
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

TEST_P(DeviceTest, CreateRenderPipeline) {
    {
        const std::pmr::string shader_code = R"""(
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

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                return float4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        )""";

        auto vs_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                            .name        = std::pmr::string(test_name),
                                                                            .type        = ShaderType::Vertex,
                                                                            .entry       = "VSMain",
                                                                            .source_code = shader_code,
                                                                        });
        ASSERT_TRUE(vs_shader);

        auto ps_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                            .name        = std::pmr::string(test_name),
                                                                            .type        = ShaderType::Pixel,
                                                                            .entry       = "PSMain",
                                                                            .source_code = shader_code,
                                                                        });
        ASSERT_TRUE(ps_shader);

        auto render_pipeline = hitagi::gfx::RenderPipeline::Create(*device, *bindings,
                                                                   {
                                                                       .name                = std::pmr::string(test_name),
                                                                       .vertex_input_layout = {
                                                                           {.semantic = "POSITION", .format = Format::R32G32B32_FLOAT, .binding = 0, .offset = 0, .stride = 0},
                                                                       },
                                                                   },
                                                                   {vs_shader, ps_shader});
        EXPECT_TRUE(render_pipeline);
    }
}

TEST_P(DeviceTest, CreateComputePipeline) {
    const std::pmr::string cs_code = R"""(
        #include "bindless.hlsl"

        [numthreads(1, 1, 1)]
        void main(uint3 thread_id : SV_DispatchThreadID) {
            
        }
    )""";

    auto cs_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                        .name        = std::pmr::string(test_name),
                                                                        .type        = ShaderType::Compute,
                                                                        .entry       = "main",
                                                                        .source_code = cs_code,
                                                                    });
    ASSERT_TRUE(cs_shader);

    auto compute_pipeline = hitagi::gfx::ComputePipeline::Create(*device, *bindings, {.name = std::pmr::string(test_name)}, cs_shader);
    EXPECT_TRUE(compute_pipeline);
}

class FenceTest : public DeviceTest {
protected:
    FenceTest() : fence(hitagi::gfx::Fence::Create(*device)) {}

    std::shared_ptr<Fence> fence;
};
INSTANTIATE_TEST_SUITE_P(
    FenceTest,
    FenceTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });
TEST_P(FenceTest, GetCurrentValue) {
    EXPECT_EQ(fence->GetCurrentValue(), 0) << "The initial value of the fence should be 0";
    auto fence_2 = hitagi::gfx::Fence::Create(*device, 1);
    EXPECT_EQ(fence_2->GetCurrentValue(), 1) << "The initial value of the fence should be 1";
}

TEST_P(FenceTest, Signal) {
    EXPECT_EQ(fence->GetCurrentValue(), 0) << "The initial value of the fence should be 0";
    fence->Signal(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after signal";
}

TEST_P(FenceTest, Wait) {
    auto signal_fn = [this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fence->Signal(1);
    };
    auto wait_fn = [this]() {
        EXPECT_TRUE(fence->Wait(1));
        EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after wait";
    };
    std::thread signal_thread(signal_fn);
    std::thread wait_thread(wait_fn);

    wait_thread.join();
    signal_thread.join();

    EXPECT_EQ(fence->GetCurrentValue(), 1) << "The value of the fence should be 1 after wait";
};

class GraphicsCommandTest : public DeviceTest {
protected:
    GraphicsCommandTest() : context(hitagi::gfx::GraphicsCommandContext::Create(*device, queues, *bindings)) {}

    void SetUp() override {
        ASSERT_TRUE(context) << "Failed to create graphics command context";
    }

    std::shared_ptr<GraphicsCommandContext> context;
};
INSTANTIATE_TEST_SUITE_P(
    GraphicsCommandTest,
    GraphicsCommandTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::format("Graphics_{}", magic_enum::enum_name(info.param));
    });

TEST_P(GraphicsCommandTest, ResourceBarrier) {
    auto buffer         = hitagi::gfx::GPUBuffer::Create(*device,
                                                         {
                                                             .name   = std::pmr::string(std::format("buffer-{}", test_name)),
                                                             .size   = 128,
                                                             .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
                                                         });
    auto render_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                       .name        = std::pmr::string(std::format("texture-{}", test_name)),
                                                                                       .width       = 128,
                                                                                       .height      = 128,
                                                                                       .format      = Format::R8G8B8A8_UNORM,
                                                                                       .clear_value = ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
                                                                                       .usages      = TextureUsageFlags::RenderTarget,
                                                                                   });

    context->Begin();
    context->ResourceBarrier(
        {},
        {{
            {
                .src_access = BarrierAccess::None,
                .dst_access = BarrierAccess::ShaderRead,
                .src_stage  = PipelineStage::None,
                .dst_stage  = PipelineStage::VertexShader,
                .buffer     = *buffer,
            },
        }},
        {{
            {
                .src_access = BarrierAccess::None,
                .dst_access = BarrierAccess::RenderTarget,
                .src_stage  = PipelineStage::None,
                .dst_stage  = PipelineStage::Render,
                .src_layout = TextureLayout::Unkown,
                .dst_layout = TextureLayout::RenderTarget,
                .texture    = *render_texture,
            },
        }});
    context->End();

    auto& queue = queues.Get(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

TEST_P(GraphicsCommandTest, PushBindlessInfo) {
    auto rotation     = rotate_z(90.0_deg);
    auto frame_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                       {
                                                           .name   = std::pmr::string(std::format("{}_buffer", test_name)),
                                                           .size   = sizeof(rotation),
                                                           .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::CopyDst,
                                                       },
                                                       {reinterpret_cast<const std::byte*>(&rotation), sizeof(rotation)});
    ASSERT_TRUE(frame_buffer != nullptr);

    struct BindlessInfo {
        BindlessHandle frame_buffer_handle;
    } bindless_info;
    auto frame_buffer_view            = CreateBufferView(*device, *bindings, frame_buffer, sizeof(rotation));
    bindless_info.frame_buffer_handle = frame_buffer_view->GetBindlessHandle();

    auto bindless_info_buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                            .name   = std::pmr::string(std::format("{}_bindless_info_buffer", test_name)),
                                                                            .size   = sizeof(BindlessInfo),
                                                                            .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                        });

    auto bindless_info_view                                   = CreateBufferView(*device, *bindings, bindless_info_buffer, sizeof(BindlessInfo));
    bindless_info_view->GetMappedSpan<BindlessInfo>().front() = bindless_info;
    BindlessHandle bindless_info_handle                       = bindless_info_view->GetBindlessHandle();

    const std::pmr::string shader_code = R"""(
            #include "bindless.hlsl"

            struct Bindless {
                hitagi::SimpleBuffer frame_constant;
            };

            struct FrameConstant {
                matrix mvp;
                matrix proj;
            };

            static const float2 positions[3] = {
                float2(0.0f, 0.5f),
                float2(0.5f, -0.5f),
                float2(-0.5f, -0.5f)
            };

            float4 VSMain(uint index: SV_VertexID) : SV_Position {
                Bindless      resource       = hitagi::load_bindless<Bindless>();
                FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();
                return mul(frame_constant.mvp, float4(positions[index], 0.0f, 1.0f));
            }

            float4 PSMain() : SV_Target {
                return float4(1.0f, 0.0f, 0.0f, 1.0f);
            }
        )""";

    auto vs_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                        .name        = std::pmr::string(std::format("{}-vs", test_name)),
                                                                        .type        = ShaderType::Vertex,
                                                                        .entry       = "VSMain",
                                                                        .source_code = shader_code,
                                                                    });
    ASSERT_TRUE(vs_shader);

    auto ps_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                        .name        = std::pmr::string(std::format("{}-ps", test_name)),
                                                                        .type        = ShaderType::Pixel,
                                                                        .entry       = "PSMain",
                                                                        .source_code = shader_code,
                                                                    });
    ASSERT_TRUE(ps_shader);

    auto pipeline = hitagi::gfx::RenderPipeline::Create(*device, *bindings,
                                                        {
                                                            .name = std::pmr::string(std::format("{}-pipeline", test_name)),
                                                        },
                                                        {vs_shader, ps_shader});
    ASSERT_TRUE(pipeline);

    context->Begin();
    context->SetPipeline(*pipeline);
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->End();

    auto& queue = queues.Get(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

class ComputeCommandTest : public DeviceTest {
protected:
    ComputeCommandTest() : context(hitagi::gfx::ComputeCommandContext::Create(*device, queues, *bindings)) {}

    void SetUp() override {
        ASSERT_TRUE(context) << "Failed to create compute command context";
    }

    std::shared_ptr<ComputeCommandContext> context;
};
INSTANTIATE_TEST_SUITE_P(
    ComputeCommandTest,
    ComputeCommandTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::format("Compute_{}", magic_enum::enum_name(info.param));
    });

TEST_P(ComputeCommandTest, ResourceBarrier) {
    auto buffer         = hitagi::gfx::GPUBuffer::Create(*device,
                                                         {
                                                             .name   = std::pmr::string(std::format("buffer-{}", test_name)),
                                                             .size   = 128,
                                                             .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::StorageWrite,
                                                         });
    auto render_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                       .name        = std::pmr::string(std::format("texture-{}", test_name)),
                                                                                       .width       = 128,
                                                                                       .height      = 128,
                                                                                       .format      = Format::R8G8B8A8_UNORM,
                                                                                       .clear_value = ClearColor{0.0f, 0.0f, 0.0f, 0.0f},
                                                                                       .usages      = TextureUsageFlags::UAV,
                                                                                   });

    context->Begin();
    context->ResourceBarrier(
        {},
        {{
            {
                .src_access = BarrierAccess::None,
                .dst_access = BarrierAccess::ShaderRead,
                .src_stage  = PipelineStage::None,
                .dst_stage  = PipelineStage::ComputeShader,
                .buffer     = *buffer,
            },
        }},
        {{
            {
                .src_access = BarrierAccess::None,
                .dst_access = BarrierAccess::ShaderWrite,
                .src_stage  = PipelineStage::None,
                .dst_stage  = PipelineStage::ComputeShader,
                .src_layout = TextureLayout::Common,
                .dst_layout = TextureLayout::ShaderWrite,
                .texture    = *render_texture,
            },
        }});
    context->End();
    auto& queue = queues.Get(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

TEST_P(ComputeCommandTest, PushBindlessInfo) {
    auto rotation     = rotate_z(90.0_deg);
    auto frame_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                       {
                                                           .name   = std::pmr::string(std::format("{}_buffer", test_name)),
                                                           .size   = sizeof(rotation),
                                                           .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::CopyDst,
                                                       },
                                                       {reinterpret_cast<const std::byte*>(&rotation), sizeof(rotation)});
    ASSERT_TRUE(frame_buffer != nullptr);

    struct BindlessInfo {
        BindlessHandle frame_buffer_handle;
    };
    auto bindless_info_buffer = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                            .name   = std::pmr::string(std::format("{}_bindless_info_buffer", test_name)),
                                                                            .size   = sizeof(BindlessInfo),
                                                                            .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                        });

    auto frame_buffer_view  = CreateBufferView(*device, *bindings, frame_buffer, sizeof(rotation));
    auto bindless_info_view = CreateBufferView(*device, *bindings, bindless_info_buffer, sizeof(BindlessInfo));
    {
        auto bindless_info                        = bindless_info_view->GetMappedSpan<BindlessInfo>();
        bindless_info.front().frame_buffer_handle = frame_buffer_view->GetBindlessHandle();
    }

    auto bindless_info_handle = bindless_info_view->GetBindlessHandle();

    const std::pmr::string cs_shader_code = R"""(
            #include "bindless.hlsl"
            struct Bindless {
                hitagi::SimpleBuffer cb;
            };

            struct Constant {
                float value;
            };

            [numthreads(1, 1, 1)]
            void main() {
                Bindless bindless = hitagi::load_bindless<Bindless>();
                Constant constant = bindless.cb.load<Constant>();
            }
        )""";

    auto cs_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                        .name        = std::pmr::string(std::format("{}-cs", test_name)),
                                                                        .type        = ShaderType::Compute,
                                                                        .entry       = "main",
                                                                        .source_code = cs_shader_code,
                                                                    });
    ASSERT_TRUE(cs_shader);

    auto pipeline = hitagi::gfx::ComputePipeline::Create(*device, *bindings, {.name = std::pmr::string(std::format("{}-pipeline", test_name))}, cs_shader);
    ASSERT_TRUE(pipeline);

    context->Begin();

    context->SetPipeline(*pipeline);
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->End();

    auto& queue = queues.Get(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

class CopyCommandTest : public DeviceTest {
protected:
    CopyCommandTest() : context(hitagi::gfx::CopyCommandContext::Create(*device, queues)) {}

    void SetUp() override {
        ASSERT_TRUE(context) << "Failed to create copy command context";
    }

    std::shared_ptr<CopyCommandContext> context;
};
INSTANTIATE_TEST_SUITE_P(
    CopyCommandTest,
    CopyCommandTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string(magic_enum::enum_name(info.param));
    });

TEST_P(CopyCommandTest, CopyBuffer) {
    constexpr std::string_view initial_data = "abcdefg";

    auto src_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                     {
                                                         .name   = std::pmr::string(std::format("{}_src", test_name)),
                                                         .size   = (sizeof(char)) * (initial_data.size()),
                                                         .usages = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
                                                     },
                                                     {
                                                         reinterpret_cast<const std::byte*>(initial_data.data()),
                                                         initial_data.size(),
                                                     });
    auto dst_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                     {
                                                         .name   = std::pmr::string(std::format("{}_dst", test_name)),
                                                         .size   = (sizeof(char)) * (initial_data.size()),
                                                         .usages = GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst,
                                                     });
    ASSERT_TRUE(src_buffer != nullptr);
    ASSERT_TRUE(dst_buffer != nullptr);

    if (context->GetType() == CommandType::Copy) {
        auto& copy_queue = queues.Get(CommandType::Copy);

        auto ctx = std::static_pointer_cast<CopyCommandContext>(context);

        ctx->Begin();
        ctx->CopyBuffer(*src_buffer, 0, *dst_buffer, 0, src_buffer->Size());
        ctx->End();

        copy_queue.Submit({{*ctx}});
        copy_queue.WaitIdle();

        auto dst_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = dst_buffer, .element_size = sizeof(char), .element_count = 0});
        auto dst_data = dst_view->GetMappedSpan<const char>();
        EXPECT_STREQ(initial_data.data(), std::string(dst_data.begin(), dst_data.end()).c_str())
            << "The content of the buffer must be the same as initial data";
    }
}

TEST_P(CopyCommandTest, CopyTexture) {
    auto src_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                    .name        = std::pmr::string(std::format("{}_src", test_name)),
                                                                                    .width       = 1024,
                                                                                    .height      = 1024,
                                                                                    .format      = Format::R32G32B32A32_FLOAT,
                                                                                    .clear_value = Color::Black(),
                                                                                    .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopySrc,
                                                                                });

    auto dst_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                    .name        = std::pmr::string(std::format("{}_dst", test_name)),
                                                                                    .width       = 1024,
                                                                                    .height      = 1024,
                                                                                    .format      = Format::R32G32B32A32_UINT,  // ! different format here
                                                                                    .clear_value = Color::Black(),
                                                                                    .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
                                                                                });

    ASSERT_TRUE(src_texture != nullptr);
    ASSERT_TRUE(dst_texture != nullptr);

    context->Begin();
    context->ResourceBarrier(
        {}, {},
        {{src_texture->Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy),
          dst_texture->Transition(BarrierAccess::CopyDst, TextureLayout::CopyDst, PipelineStage::Copy)}});
    context->CopyTextureRegion(*src_texture, {0, 0, 0}, *dst_texture, {0, 0, 0}, {1024, 1024, 1});
    context->End();

    EXPECT_NO_THROW({
        auto& copy_queue = queues.Get(CommandType::Copy);
        copy_queue.Submit({{*context}});
        copy_queue.WaitIdle();
    });
}

TEST_P(CopyCommandTest, CopyTextureToBuffer) {
    constexpr std::uint32_t tex_width  = 4;
    constexpr std::uint32_t tex_height = 4;
    constexpr auto          tex_format = Format::R8G8B8A8_UNORM;
    constexpr std::size_t   pixel_size = 4;

    std::array<R8G8B8A8Unorm, tex_width * tex_height> pixels;
    for (std::uint32_t i = 0; i < pixels.size(); ++i) {
        auto val  = static_cast<std::uint8_t>(i);
        pixels[i] = R8G8B8A8Unorm(val, val, val, 0xFF);
    }

    auto src_texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                    {
                                                        .name   = std::pmr::string(std::format("{}_src", test_name)),
                                                        .width  = tex_width,
                                                        .height = tex_height,
                                                        .format = tex_format,
                                                        .usages = TextureUsageFlags::SRV | TextureUsageFlags::CopySrc | TextureUsageFlags::CopyDst,
                                                    },
                                                    {reinterpret_cast<const std::byte*>(pixels.data()), sizeof(pixels)});
    ASSERT_TRUE(src_texture != nullptr);

    auto dst_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                     {
                                                         .name   = std::pmr::string(std::format("{}_dst", test_name)),
                                                         .size   = (pixel_size) * (tex_width * tex_height),
                                                         .usages = GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst,
                                                     });
    ASSERT_TRUE(dst_buffer != nullptr);

    context->Begin();
    context->ResourceBarrier(
        {}, {},
        {{src_texture->Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy)}});
    context->CopyTextureToBuffer(*src_texture, {0, 0, 0}, {tex_width, tex_height, 1}, *dst_buffer, 0);
    context->End();

    auto& copy_queue = queues.Get(CommandType::Copy);
    copy_queue.Submit({{*context}});
    copy_queue.WaitIdle();

    auto readback_view = hitagi::gfx::GPUBufferView::Create(*device, *bindings, {.buffer = dst_buffer, .element_size = sizeof(R8G8B8A8Unorm), .element_count = 0});
    auto readback      = readback_view->GetMappedSpan<const R8G8B8A8Unorm>();
    for (std::uint32_t i = 0; i < pixels.size(); ++i) {
        auto val = static_cast<std::uint8_t>(i);
        EXPECT_EQ(readback[i][0], val) << "pixel R mismatch at index " << i;
        EXPECT_EQ(readback[i][1], val) << "pixel G mismatch at index " << i;
        EXPECT_EQ(readback[i][2], val) << "pixel B mismatch at index " << i;
        EXPECT_EQ(readback[i][3], 0xFF) << "pixel A mismatch at index " << i;
    }

    const auto output_path = std::filesystem::path("temp") /
                             std::format("CopyCommandTest.CopyTextureToBuffer_{}.png", magic_enum::enum_name(GetParam()));
    std::filesystem::create_directories(output_path.parent_path());

    hitagi::asset::Texture image(
        tex_width,
        tex_height,
        tex_format,
        hitagi::core::Buffer(std::span{reinterpret_cast<const std::byte*>(readback.data()), static_cast<std::size_t>(dst_buffer->Size())}));
    const auto png = hitagi::asset::PngEncoder{}.Encode(image);
    ASSERT_FALSE(png.Empty());
    hitagi::core::FileIOManager{}.SaveBuffer(png, output_path);
}

class SwapChainTest : public DeviceTest {
protected:
    SwapChainTest()
        : DeviceTest(),
          app(hitagi::Application::CreateApp(hitagi::AppConfig{
              .title     = std::pmr::string{std::format("App/{}", test_name)},
              .log_level = spdlog::level::to_string_view(spdlog::get_level()).data(),
              .headless  = true,
          })) {}

    void SetUp() override {
        ASSERT_TRUE(app != nullptr);
        swap_chain = hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics), {
                                                                                                                 .name   = std::pmr::string(test_name),
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
    auto rect = app->GetWindowRect();
    EXPECT_EQ(swap_chain->GetWidth(), rect.right - rect.left) << "swap chain should be same size as window";
    EXPECT_EQ(swap_chain->GetHeight(), rect.bottom - rect.top) << "swap chain should be same size as window";
}

TEST_P(SwapChainTest, SwapChainResizing) {
    auto rect = app->GetWindowRect();

    // Resize swap chain
    app->ResizeWindow(800, 600);
    rect = app->GetWindowRect();
    swap_chain->Resize();

    EXPECT_EQ(swap_chain->GetWidth(), rect.right - rect.left) << "Swap chain should be same size as window after resizing";
    EXPECT_EQ(swap_chain->GetHeight(), rect.bottom - rect.top) << "Swap chain should be same size as window after resizing";
}

TEST_P(DeviceTest, DrawTriangle) {
    auto app = hitagi::Application::CreateApp(
        hitagi::AppConfig{
            .title     = std::pmr::string{std::format("App/{}", test_name)},
            .log_level = spdlog::level::to_string_view(spdlog::get_level()).data(),
            .headless  = true,
        });

    auto rect = app->GetWindowRect();

    auto swap_chain = hitagi::gfx::SwapChain::Create(*device, queues.Get(hitagi::gfx::CommandType::Graphics),
                                                     {
                                                         .name   = UnitTest::GetInstance()->current_test_info()->name(),
                                                         .window = app->GetWindow(),
                                                     });

    const std::pmr::string shader_code = R"""(
            #include "bindless.hlsl"

            struct Bindless {
                hitagi::SimpleBuffer constant;
                hitagi::Texture      texture;
                hitagi::Sampler      sampler;
                uint                 padding;
            };

            struct Constant {
                matrix rotation;
            };
        
            struct VS_INPUT {
                float3 pos : POSITION;
                float3 col : COLOR;
            };

            struct PS_INPUT {
                float4 pos : SV_POSITION;
                float3 col : COLOR;
            };

            PS_INPUT VSMain(VS_INPUT input) {
                Bindless bindless = hitagi::load_bindless<Bindless>();
                Constant constant = bindless.constant.load<Constant>();

                PS_INPUT output;
                output.pos = mul(constant.rotation, float4(input.pos, 1.0f));
                output.col = input.col;
                return output;
            }

            float4 PSMain(PS_INPUT input) : SV_TARGET {
                Bindless     bindless = hitagi::load_bindless<Bindless>();
                SamplerState sampler  = bindless.sampler.load();
                const float4 color = bindless.texture.sample<float4>(sampler, input.col.xy);
                return color;
            }
        )""";

    auto vertex_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                            .name        = std::pmr::string(std::format("VS-{}", test_name)),
                                                                            .type        = ShaderType::Vertex,
                                                                            .entry       = "VSMain",
                                                                            .source_code = shader_code,
                                                                        });
    ASSERT_TRUE(vertex_shader);

    auto pixel_shader = hitagi::gfx::Shader::Create(*device, compiler, {
                                                                           .name        = std::pmr::string(std::format("PS-{}", test_name)),
                                                                           .type        = ShaderType::Pixel,
                                                                           .entry       = "PSMain",
                                                                           .source_code = shader_code,
                                                                       });
    ASSERT_TRUE(pixel_shader);

    auto pipeline = hitagi::gfx::RenderPipeline::Create(*device, *bindings,
                                                        {
                                                            .name                = std::pmr::string(std::format("pipeline-{}", test_name)),
                                                            .vertex_input_layout = {
                                                                {
                                                                    .semantic = "POSITION",
                                                                    .format   = Format::R32G32B32_FLOAT,
                                                                    .binding  = 0,
                                                                    .offset   = 0,
                                                                    .stride   = 2 * sizeof(vec3f),
                                                                },
                                                                {
                                                                    .semantic = "COLOR",
                                                                    .format   = Format::R32G32B32_FLOAT,
                                                                    .binding  = 0,
                                                                    .offset   = sizeof(vec3f),
                                                                    .stride   = 2 * sizeof(vec3f),
                                                                },
                                                            },
                                                            .render_format = Format::R8G8B8A8_UNORM,
                                                        },
                                                        {vertex_shader, pixel_shader});

    auto offscreen_render_target = hitagi::gfx::Texture::Create(*device, queues, *bindings, {
                                                                                                .name        = std::pmr::string(std::format("{}-OffscreenRenderTarget", test_name)),
                                                                                                .width       = static_cast<std::uint32_t>(rect.right - rect.left),
                                                                                                .height      = static_cast<std::uint32_t>(rect.bottom - rect.top),
                                                                                                .format      = Format::R8G8B8A8_UNORM,
                                                                                                .clear_value = ClearColor{0.0f, 0.0f, 0.0f, 1.0f},
                                                                                                .usages      = TextureUsageFlags::RenderTarget | TextureUsageFlags::CopySrc,
                                                                                            });
    ASSERT_TRUE(offscreen_render_target);
    auto offscreen_render_target_view = hitagi::gfx::TextureView::Create(*device, *bindings, {
                                                                                                 .name    = std::pmr::string(std::format("{}-OffscreenRenderTargetView", test_name)),
                                                                                                 .texture = offscreen_render_target,
                                                                                                 .type    = TextureViewType::RenderTarget,
                                                                                             });
    ASSERT_TRUE(offscreen_render_target_view);

    // clang-format off
        constexpr std::array<vec3f, 6> triangle = {{
            /*         pos       */  /*    color     */
            {-0.25f, -0.25f, 0.00f}, {1.0f, 0.0f, 0.0f},  // point 0
            { 0.00f,  0.25f, 0.00f}, {0.0f, 1.0f, 0.0f},  // point 1
            { 0.25f, -0.25f, 0.00f}, {0.0f, 0.0f, 1.0f},  // point 2
        }};
    // clang-format on
        auto vertex_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                            {
                                                                .name   = "I know DirectX12 positions buffer",
                                                                .size   = (sizeof(vec3f)) * (triangle.size()),
                                                                .usages = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
                                                            },
                                                            {reinterpret_cast<const std::byte*>(triangle.data()), sizeof(triangle)});

        std::array pink_color = {
            R8G8B8A8Unorm(0xa9, 0xC0, 0x61, 0xFF),
            R8G8B8A8Unorm(0x28, 0xC0, 0xCB, 0xFF),
            R8G8B8A8Unorm(0x31, 0xC0, 0x34, 0xFF),
            R8G8B8A8Unorm(0x00, 0x90, 0xCB, 0xFF),
        };

        auto texture = hitagi::gfx::Texture::Create(*device, queues, *bindings,
                                                    {
                                                        .name   = std::pmr::string(std::format("Texture-{}", test_name)),
                                                        .width  = 2,
                                                        .height = 2,
                                                        .format = Format::R8G8B8A8_UNORM,
                                                        .usages = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
                                                    },
                                                    {reinterpret_cast<const std::byte*>(pink_color.data()), sizeof(pink_color)});

        auto sampler      = hitagi::gfx::Sampler::Create(*device, *bindings, {
                                                                                 .name = std::pmr::string(std::format("Sampler-{}", test_name)),
                                                                             });
        auto texture_view = hitagi::gfx::TextureView::Create(*device, *bindings, {
                                                                                     .name    = std::pmr::string(std::format("{}-TextureView", test_name)),
                                                                                     .texture = texture,
                                                                                     .type    = TextureViewType::ShaderRead,
                                                                                 });

        struct Constant {
            mat4f rotation;
        };
        auto constant_buffer                                             = hitagi::gfx::GPUBuffer::Create(*device, {
                                                                                                                       .name   = std::pmr::string(std::format("{}-Buffer", test_name)),
                                                                                                                       .size   = sizeof(Constant),
                                                                                                                       .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite,
                                                                                                                   });
        auto constant_buffer_view                                        = CreateBufferView(*device, *bindings, constant_buffer, sizeof(Constant));
        constant_buffer_view->GetMappedSpan<Constant>().front().rotation = translate(vec3f(.5f, 0, 0)) * rotate_z<float>(20.0_deg);

        struct BindlessInfo {
            BindlessHandle constant_buffer;
            BindlessHandle texture;
            BindlessHandle sampler;
            uint32_t       padding;
        };

    BindlessInfo bindless_info{
        .constant_buffer = constant_buffer_view->GetBindlessHandle(),
        .texture         = texture_view->GetBindlessHandle(),
        .sampler         = sampler->GetBindlessHandle(),
    };

    auto bindless_info_buffer = hitagi::gfx::GPUBuffer::Create(*device,
                                                               {
                                                                   .name   = std::pmr::string(std::format("{}-BindlessHandles", test_name)),
                                                                   .size   = sizeof(BindlessInfo),
                                                                   .usages = GPUBufferUsageFlags::StorageRead | GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopyDst,
                                                               },
                                                               {reinterpret_cast<const std::byte*>(&bindless_info), sizeof(bindless_info)});

    auto bindless_info_view   = CreateBufferView(*device, *bindings, bindless_info_buffer, sizeof(BindlessInfo));
    auto bindless_info_handle = bindless_info_view->GetBindlessHandle();

    auto& gfx_queue = queues.Get(CommandType::Graphics);

    // while (!app->IsQuit()) {
    auto context = hitagi::gfx::GraphicsCommandContext::Create(*device, queues, *bindings, "I know DirectX12 context");
    context->Begin();
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
    context->ResourceBarrier(
        {}, {},
        {{offscreen_render_target->Transition(BarrierAccess::RenderTarget, TextureLayout::RenderTarget, PipelineStage::Render)}});

    context->BeginRendering(*offscreen_render_target_view, {}, true);
    context->SetPipeline(*pipeline);
    context->SetVertexBuffers(0, {{*vertex_buffer}}, {{0}});
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->Draw(3);
    context->EndRendering();

    context->ResourceBarrier(
        {}, {},
        std::array{offscreen_render_target->Transition(BarrierAccess::CopySrc, TextureLayout::CopySrc, PipelineStage::Copy)});
    context->End();

    gfx_queue.Submit({{*context}});
    gfx_queue.WaitIdle();

    app->Tick();
    // }

    auto       pixels      = readback_texture(*device, queues, *bindings, *offscreen_render_target);
    const auto output_path = std::filesystem::path("temp") / std::format("DeviceTest.DrawTriangle_{}.png", magic_enum::enum_name(GetParam()));
    std::filesystem::create_directories(output_path.parent_path());

    hitagi::asset::Texture image(
        offscreen_render_target->GetDesc().width,
        offscreen_render_target->GetDesc().height,
        offscreen_render_target->GetDesc().format,
        std::move(pixels));
    const auto png = hitagi::asset::PngEncoder{}.Encode(image);
    ASSERT_FALSE(png.Empty());
    hitagi::core::FileIOManager{}.SaveBuffer(png, output_path);

}
