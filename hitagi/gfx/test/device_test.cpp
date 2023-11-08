#include <hitagi/core/buffer.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/application.hpp>
#include <hitagi/utils/test.hpp>
#include <hitagi/utils/flags.hpp>
#include <numeric>

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
          device(Device::Create(GetParam(), test_name)) {}

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
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
    EXPECT_THROW(device->CreateGPUBuffer({
                     .name          = test_name,
                     .element_size  = 0,
                     .element_count = 1,
                 }),
                 std::invalid_argument)
        << "should throw exception when element_size is 0";

    EXPECT_THROW(device->CreateGPUBuffer({
                     .name          = test_name,
                     .element_size  = 1,
                     .element_count = 0,
                 }),
                 std::invalid_argument)
        << "should throw exception when element_count is 0";

    EXPECT_TRUE(device->CreateGPUBuffer({
        .name          = test_name,
        .element_size  = sizeof(vec3f),
        .element_count = 1024,
        .usages        = GPUBufferUsageFlags::Vertex,
    }))
        << "should create vertex buffer successfully";

    EXPECT_TRUE(device->CreateGPUBuffer({
        .name          = test_name,
        .element_size  = sizeof(std::uint32_t),
        .element_count = 1024,
        .usages        = GPUBufferUsageFlags::Index,
    }))
        << "should create index buffer successfully";

    EXPECT_TRUE(device->CreateGPUBuffer({
        .name          = test_name,
        .element_size  = sizeof(std::uint16_t),
        .element_count = 1024,
        .usages        = GPUBufferUsageFlags::Index,
    }))
        << "should create index buffer successfully";

    EXPECT_THROW(device->CreateGPUBuffer({
                     .name          = test_name,
                     .element_size  = sizeof(std::uint8_t),
                     .element_count = 1024,
                     .usages        = GPUBufferUsageFlags::Index,
                 }),
                 std::invalid_argument)
        << "can not create index buffer without uint16_t or uint32_t.";

    auto constant_buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(mat4f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant,
        });
    EXPECT_TRUE(constant_buffer) << "should create constant buffer successfully.";

    if (device->device_type == Device::Type::DX12) {
        EXPECT_EQ(constant_buffer->AlignedElementSize(), 256) << "DX12 constant buffer should be aligned to 256 bytes.";
    } else {
        EXPECT_EQ(constant_buffer->AlignedElementSize(), sizeof(mat4f));
    }

    EXPECT_TRUE(device->CreateGPUBuffer({
        .name          = test_name,
        .element_size  = sizeof(vec4f),
        .element_count = 1024,
        .usages        = GPUBufferUsageFlags::Storage,
    })) << "should create storage buffer successfully.";
}

TEST_P(GPUBufferTest, Mapping) {
    auto mapped_buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(vec3f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
        });
    ASSERT_TRUE(mapped_buffer);
    EXPECT_TRUE(mapped_buffer->Map()) << "should map successfully.";
    EXPECT_NO_THROW(mapped_buffer->UnMap()) << "should unmap successfully.";

    auto no_mapped_buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(vec3f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant,
        });
    ASSERT_TRUE(no_mapped_buffer);
    EXPECT_THROW(no_mapped_buffer->Map(), std::runtime_error)
        << "can not map buffer without usage"
        << magic_enum::enum_flags_name(GPUBufferUsageFlags::MapWrite)
        << "or"
        << magic_enum::enum_flags_name(GPUBufferUsageFlags::MapRead);
    EXPECT_THROW(no_mapped_buffer->UnMap(), std::runtime_error) << "can not unmap buffer without mapping.";
}

TEST_P(GPUBufferTest, CreateBufferView) {
    auto buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(vec3f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
        });
    ASSERT_TRUE(buffer);
    EXPECT_NO_THROW(GPUBufferView<vec3f> buffer_view(*buffer));
    EXPECT_NO_THROW(GPUBufferView<const vec3f> buffer_view(*buffer));

    auto no_map_written_buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(vec3f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapRead,
        });
    ASSERT_TRUE(no_map_written_buffer);
    EXPECT_NO_THROW(GPUBufferView<const vec3f> buffer_view(*buffer));
    EXPECT_THROW(GPUBufferView<vec3f> buffer_view(*no_map_written_buffer), std::invalid_argument)
        << "can not create not constant buffer view from buffer without flag "
        << flags_name(GPUBufferUsageFlags::MapWrite);

    auto no_mapped_buffer = device->CreateGPUBuffer(
        {
            .name          = test_name,
            .element_size  = sizeof(vec3f),
            .element_count = 1024,
            .usages        = GPUBufferUsageFlags::Constant,
        });
    ASSERT_TRUE(no_mapped_buffer);
    EXPECT_THROW(GPUBufferView<vec3f> buffer_view(*no_mapped_buffer), std::invalid_argument)
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
        .name          = test_name,
        .element_size  = sizeof(int),
        .element_count = 1024,
        .usages        = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::CopyDst | GPUBufferUsageFlags::MapRead,
    };

    auto buffer = device->CreateGPUBuffer(desc, data_span);
    ASSERT_TRUE(buffer);
    auto buffer_view = GPUBufferView<const int>(*buffer);
    for (auto i = 0; i < 1024; ++i) {
        EXPECT_EQ(buffer_view[i], i) << "buffer data not match at index " << i;
    }

    desc.usages = GPUBufferUsageFlags::Constant;
    EXPECT_THROW(device->CreateGPUBuffer(desc, data_span), std::invalid_argument)
        << "can not create buffer with initial data without flag "
        << flags_name(GPUBufferUsageFlags::CopyDst);
}

TEST_P(DeviceTest, CreateGraphicsCommandContext) {
    auto context = device->CreateGraphicsContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create graphics context";
}

TEST_P(DeviceTest, CreateComputeCommandContext) {
    auto context = device->CreateComputeContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create compute context";
}

TEST_P(DeviceTest, CreateCopyCommandContext) {
    auto context = device->CreateCopyContext();
    EXPECT_TRUE(context != nullptr) << "Failed to create copy context";
}

TEST_P(DeviceTest, CreateTexture1D) {
    auto texture = device->CreateTexture(
        {
            .name        = test_name,
            .width       = 128,
            .format      = Format::R8G8B8A8_UNORM,
            .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
        });
    EXPECT_TRUE(texture != nullptr);
}

TEST_P(DeviceTest, CreateTexture2D) {
    // Buffer data(128 * 128 * sizeof(vec4f));

    // for (int i = 0; i < 1000; i++) {
    //     auto texture = device->CreateTexture(
    //         {
    //             .name        = test_name,
    //             .width       = 128,
    //             .height      = 128,
    //             .format      = Format::R8G8B8A8_UNORM,
    //             .clear_value = {vec4f(1.0f, 1.0f, 1.0f, 1.0f)},
    //             .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
    //         },
    //         data.Span<const std::byte>());

    //     EXPECT_TRUE(texture != nullptr);
    // }
    device->Profile(0);
}

TEST_P(DeviceTest, CreateTexture3D) {
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

TEST_P(DeviceTest, CreateTexture2DArray) {
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

TEST_P(DeviceTest, CreateSampler) {
    auto sampler = device->CreateSampler(
        {
            .name       = test_name,
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

        auto vs_shader = device->CreateShader({
            .name        = test_name,
            .type        = ShaderType::Vertex,
            .entry       = "VSMain",
            .source_code = shader_code,
        });
        ASSERT_TRUE(vs_shader);

        auto ps_shader = device->CreateShader({
            .name        = test_name,
            .type        = ShaderType::Pixel,
            .entry       = "PSMain",
            .source_code = shader_code,
        });
        ASSERT_TRUE(ps_shader);

        auto render_pipeline = device->CreateRenderPipeline(
            {
                .name                = test_name,
                .shaders             = {vs_shader, ps_shader},
                .vertex_input_layout = {
                    {"POSITION", Format::R32G32B32_FLOAT, 0, 0, 0},
                },
            });
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

    auto cs_shader = device->CreateShader({
        .name        = test_name,
        .type        = ShaderType::Compute,
        .entry       = "main",
        .source_code = cs_code,
    });
    ASSERT_TRUE(cs_shader);

    auto compute_pipeline = device->CreateComputePipeline({
        .name = test_name,
        .cs   = cs_shader,
    });
    EXPECT_TRUE(compute_pipeline);
}

class FenceTest : public DeviceTest {
protected:
    FenceTest() : fence(device->CreateFence()) {}

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
    auto fence_2 = device->CreateFence(1);
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
    GraphicsCommandTest() : context(device->CreateGraphicsContext()) {}

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
        return fmt::format("Graphics_{}", magic_enum::enum_name(info.param));
    });

TEST_P(GraphicsCommandTest, ResourceBarrier) {
    auto buffer = device->CreateGPUBuffer(
        {
            .name         = std::pmr::string(fmt::format("buffer-{}", test_name)),
            .element_size = 128,
            .usages       = GPUBufferUsageFlags::Constant,
        });
    auto render_texture = device->CreateTexture({
        .name        = std::pmr::string(fmt::format("texture-{}", test_name)),
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
                .dst_access = BarrierAccess::Constant,
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

    auto& queue = device->GetCommandQueue(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

TEST_P(GraphicsCommandTest, PushBindlessInfo) {
    auto rotation     = rotate_z(90.0_deg);
    auto frame_buffer = device->CreateGPUBuffer(
        {
            .name         = std::pmr::string(fmt::format("{}_buffer", test_name)),
            .element_size = sizeof(rotation),
            .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(&rotation), sizeof(rotation)});
    ASSERT_TRUE(frame_buffer != nullptr);

    struct BindlessInfo {
        BindlessHandle frame_buffer_handle;
    } bindless_info;
    bindless_info.frame_buffer_handle = device->GetBindlessUtils().CreateBindlessHandle(*frame_buffer, 0);

    auto bindless_info_buffer = device->CreateGPUBuffer({
        .name         = std::pmr::string(fmt::format("{}_bindless_info_buffer", test_name)),
        .element_size = sizeof(BindlessInfo),
        .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    GPUBufferView<BindlessInfo>(*bindless_info_buffer).front() = bindless_info;

    BindlessHandle bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*bindless_info_buffer, 0);

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

    auto vs_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("{}-vs", test_name)),
        .type        = ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(vs_shader);

    auto ps_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("{}-ps", test_name)),
        .type        = ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(ps_shader);

    auto pipeline = device->CreateRenderPipeline({
        .name    = std::pmr::string(fmt::format("{}-pipeline", test_name)),
        .shaders = {vs_shader, ps_shader},
    });
    ASSERT_TRUE(pipeline);

    context->Begin();
    context->SetPipeline(*pipeline);
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->End();

    auto& queue = device->GetCommandQueue(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();

    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info_handle);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.frame_buffer_handle);
}

class ComputeCommandTest : public DeviceTest {
protected:
    ComputeCommandTest() : context(device->CreateComputeContext()) {}

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
        return fmt::format("Compute_{}", magic_enum::enum_name(info.param));
    });

TEST_P(ComputeCommandTest, ResourceBarrier) {
    auto buffer = device->CreateGPUBuffer(
        {
            .name         = std::pmr::string(fmt::format("buffer-{}", test_name)),
            .element_size = 128,
            .usages       = GPUBufferUsageFlags::Storage,
        });
    auto render_texture = device->CreateTexture({
        .name        = std::pmr::string(fmt::format("texture-{}", test_name)),
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
                .dst_access = BarrierAccess::Constant,
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
    auto& queue = device->GetCommandQueue(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();
}

TEST_P(ComputeCommandTest, PushBindlessInfo) {
    auto rotation     = rotate_z(90.0_deg);
    auto frame_buffer = device->CreateGPUBuffer(
        {
            .name         = std::pmr::string(fmt::format("{}_buffer", test_name)),
            .element_size = sizeof(rotation),
            .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(&rotation), sizeof(rotation)});
    ASSERT_TRUE(frame_buffer != nullptr);

    struct BindlessInfo {
        BindlessHandle frame_buffer_handle;
    };
    auto bindless_info_buffer = device->CreateGPUBuffer({
        .name         = std::pmr::string(fmt::format("{}_bindless_info_buffer", test_name)),
        .element_size = sizeof(BindlessInfo),
        .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });

    auto& bindless_info               = GPUBufferView<BindlessInfo>(*bindless_info_buffer).front();
    bindless_info.frame_buffer_handle = device->GetBindlessUtils().CreateBindlessHandle(*frame_buffer, 0);

    auto bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*bindless_info_buffer, 0);

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

    auto cs_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("{}-cs", test_name)),
        .type        = ShaderType::Compute,
        .entry       = "main",
        .source_code = cs_shader_code,
    });
    ASSERT_TRUE(cs_shader);

    auto pipeline = device->CreateComputePipeline({
        .name = std::pmr::string(fmt::format("{}-pipeline", test_name)),
        .cs   = cs_shader,
    });
    ASSERT_TRUE(pipeline);

    context->Begin();

    context->SetPipeline(*pipeline);
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->End();

    auto& queue = device->GetCommandQueue(context->GetType());
    queue.Submit({{*context}});
    queue.WaitIdle();

    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info_handle);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.frame_buffer_handle);
}

class CopyCommandTest : public DeviceTest {
protected:
    CopyCommandTest() : context(device->CreateCopyContext()) {}

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

    auto src_buffer = device->CreateGPUBuffer(
        {
            .name          = std::pmr::string(fmt::format("{}_src", test_name)),
            .element_size  = sizeof(char),
            .element_count = initial_data.size(),
            .usages        = GPUBufferUsageFlags::MapWrite | GPUBufferUsageFlags::CopySrc,
        },
        {
            reinterpret_cast<const std::byte*>(initial_data.data()),
            initial_data.size(),
        });
    auto dst_buffer = device->CreateGPUBuffer(
        {
            .name          = std::pmr::string(fmt::format("{}_dst", test_name)),
            .element_size  = sizeof(char),
            .element_count = initial_data.size(),
            .usages        = GPUBufferUsageFlags::MapRead | GPUBufferUsageFlags::CopyDst,
        });
    ASSERT_TRUE(src_buffer != nullptr);
    ASSERT_TRUE(dst_buffer != nullptr);

    if (context->GetType() == CommandType::Copy) {
        auto& copy_queue = device->GetCommandQueue(CommandType::Copy);

        auto ctx = std::static_pointer_cast<CopyCommandContext>(context);

        ctx->Begin();
        ctx->CopyBuffer(*src_buffer, 0, *dst_buffer, 0, src_buffer->Size());
        ctx->End();

        copy_queue.Submit({{*ctx}});
        copy_queue.WaitIdle();

        auto dst_data = GPUBufferView<const char>(*dst_buffer);
        EXPECT_STREQ(initial_data.data(), std::string(dst_data.begin(), dst_data.end()).c_str())
            << "The content of the buffer must be the same as initial data";
    }
}

TEST_P(CopyCommandTest, CopyTexture) {
    auto src_texture = device->CreateTexture({
        .name        = std::pmr::string(fmt::format("{}_src", test_name)),
        .width       = 1024,
        .height      = 1024,
        .format      = Format::R32G32B32A32_FLOAT,
        .clear_value = vec4f{0.0f, 0.0f, 0.0f, 1.0f},
        .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopySrc,
    });

    auto dst_texture = device->CreateTexture({
        .name        = std::pmr::string(fmt::format("{}_dst", test_name)),
        .width       = 1024,
        .height      = 1024,
        .format      = Format::R32G32B32A32_UINT,  // ! different format here
        .clear_value = vec4f{0.0f, 0.0f, 0.0f, 1.0f},
        .usages      = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
    });

    ASSERT_TRUE(src_texture != nullptr);
    ASSERT_TRUE(dst_texture != nullptr);

    context->Begin();
    context->ResourceBarrier(
        {}, {},
        {{TextureBarrier{
              .src_access = BarrierAccess::None,
              .dst_access = BarrierAccess::CopySrc,
              .src_stage  = PipelineStage::None,
              .dst_stage  = PipelineStage::Copy,
              .src_layout = TextureLayout::Unkown,
              .dst_layout = TextureLayout::CopySrc,
              .texture    = *src_texture,
          },
          TextureBarrier{
              .src_access = BarrierAccess::None,
              .dst_access = BarrierAccess::CopyDst,
              .src_stage  = PipelineStage::None,
              .dst_stage  = PipelineStage::Copy,
              .src_layout = TextureLayout::Unkown,
              .dst_layout = TextureLayout::CopyDst,
              .texture    = *dst_texture,
          }}});
    context->CopyTextureRegion(*src_texture, {0, 0, 0}, *dst_texture, {0, 0, 0}, {1024, 1024, 1});
    context->End();

    EXPECT_NO_THROW({
        auto& copy_queue = device->GetCommandQueue(CommandType::Copy);
        copy_queue.Submit({{*context}});
        copy_queue.WaitIdle();
    });
}

class SwapChainTest : public DeviceTest {
protected:
    SwapChainTest()
        : DeviceTest(),
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

TEST_P(SwapChainTest, SwapChainResizing) {
    auto rect = app->GetWindowsRect();

    // Resize swap chain
    app->ResizeWindow(800, 600);
    rect = app->GetWindowsRect();
    swap_chain->Resize();

    EXPECT_EQ(swap_chain->GetWidth(), rect.right - rect.left) << "Swap chain should be same size as window after resizing";
    EXPECT_EQ(swap_chain->GetHeight(), rect.bottom - rect.top) << "Swap chain should be same size as window after resizing";
}

TEST_P(DeviceTest, DrawTriangle) {
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

    auto vertex_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("VS-{}", test_name)),
        .type        = ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(vertex_shader);

    auto pixel_shader = device->CreateShader({
        .name        = std::pmr::string(fmt::format("PS-{}", test_name)),
        .type        = ShaderType::Pixel,
        .entry       = "PSMain",
        .source_code = shader_code,
    });
    ASSERT_TRUE(pixel_shader);

    auto pipeline = device->CreateRenderPipeline({
        .name    = std::pmr::string(fmt::format("pipeline-{}", test_name)),
        .shaders = {
            vertex_shader,
            pixel_shader,
        },
        .vertex_input_layout = {
            {"POSITION", Format::R32G32B32_FLOAT, 0, 0, 2 * sizeof(vec3f)},
            {"COLOR", Format::R32G32B32_FLOAT, 0, sizeof(vec3f), 2 * sizeof(vec3f)},
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
            .element_size  = sizeof(vec3f),
            .element_count = triangle.size(),
            .usages        = GPUBufferUsageFlags::Vertex | GPUBufferUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(triangle.data()), sizeof(triangle)});

    std::array pink_color = {
        R8G8B8A8Unorm(0xa9, 0xC0, 0x61, 0xFF),
        R8G8B8A8Unorm(0x28, 0xC0, 0xCB, 0xFF),
        R8G8B8A8Unorm(0x31, 0xC0, 0x34, 0xFF),
        R8G8B8A8Unorm(0x00, 0x90, 0xCB, 0xFF),
    };

    auto texture = device->CreateTexture(
        {
            .name   = std::pmr::string(fmt::format("Texture-{}", test_name)),
            .width  = 2,
            .height = 2,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::SRV | TextureUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(pink_color.data()), sizeof(pink_color)});

    auto sampler = device->CreateSampler({
        .name = std::pmr::string(fmt::format("Sampler-{}", test_name)),
    });

    struct Constant {
        mat4f rotation;
    };
    auto constant_buffer                           = device->CreateGPUBuffer({
                                  .name         = std::pmr::string(fmt::format("{}-ConstantBuffer", test_name)),
                                  .element_size = sizeof(Constant),
                                  .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
    });
    GPUBufferView<mat4f>(*constant_buffer).front() = translate(vec3f(.5f, 0, 0)) * rotate_z<float>(20.0_deg);

    struct BindlessInfo {
        BindlessHandle constant_buffer;
        BindlessHandle texture;
        BindlessHandle sampler;
        uint32_t       padding;
    };

    BindlessInfo bindless_info{
        .constant_buffer = device->GetBindlessUtils().CreateBindlessHandle(*constant_buffer, 0),
        .texture         = device->GetBindlessUtils().CreateBindlessHandle(*texture),
        .sampler         = device->GetBindlessUtils().CreateBindlessHandle(*sampler),
    };

    auto bindless_info_buffer = device->CreateGPUBuffer(
        {
            .name         = std::pmr::string(fmt::format("{}-BindlessHandles", test_name)),
            .element_size = sizeof(BindlessInfo),
            .usages       = GPUBufferUsageFlags::Constant | GPUBufferUsageFlags::MapWrite,
        },
        {reinterpret_cast<const std::byte*>(&bindless_info), sizeof(bindless_info)});

    auto bindless_info_handle = device->GetBindlessUtils().CreateBindlessHandle(*bindless_info_buffer, 0);

    auto& gfx_queue = device->GetCommandQueue(CommandType::Graphics);

    // while (!app->IsQuit()) {
    auto context = device->CreateGraphicsContext("I know DirectX12 context");
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
    auto& render_target = swap_chain->AcquireTextureForRendering();

    ;
    context->ResourceBarrier(
        {}, {},
        {{TextureBarrier{
            .src_access = BarrierAccess::None,
            .dst_access = BarrierAccess::RenderTarget,
            .src_stage  = PipelineStage::Render,
            .dst_stage  = PipelineStage::Render,
            .src_layout = TextureLayout::Unkown,
            .dst_layout = TextureLayout::RenderTarget,
            .texture    = render_target,
        }}});

    context->BeginRendering(render_target);
    context->SetPipeline(*pipeline);
    context->SetVertexBuffers(0, {{*vertex_buffer}}, {{0}});
    context->PushBindlessMetaInfo({
        .handle = bindless_info_handle,
    });
    context->Draw(3);
    context->EndRendering();

    context->ResourceBarrier(
        {}, {},
        std::array{TextureBarrier{
            .src_access = BarrierAccess::RenderTarget,
            .dst_access = BarrierAccess::Present,
            .src_stage  = PipelineStage::Render,
            .dst_stage  = PipelineStage::All,
            .src_layout = TextureLayout::RenderTarget,
            .dst_layout = TextureLayout::Present,
            .texture    = render_target,
        }});
    context->End();

    gfx_queue.Submit({{*context}});
    swap_chain->Present();
    gfx_queue.WaitIdle();

    app->Tick();
    // }

    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.sampler);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.texture);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info.constant_buffer);
    device->GetBindlessUtils().DiscardBindlessHandle(bindless_info_handle);
}

int main(int argc, char** argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}