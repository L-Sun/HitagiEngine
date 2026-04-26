#include "test_macros.hpp"
#include <spdlog/spdlog.h>

import std;
import magic_enum;
import gfx;
import asset;
import core;
import math;
import utils;
import test_utils;

using namespace testing;
using namespace hitagi;
using namespace hitagi::core;
using namespace hitagi::gfx;
using namespace hitagi::math;

TEST(GfxTest, DescHash) {
    {
        GPUBufferDesc
            buffer_desc_1 = {},
            buffer_desc_2 = {
                .name          = "buffer_desc_1",
                .element_size  = 16,
                .element_count = 32,
                .usages        = GPUBufferUsageFlags::Vertex,
            };

        buffer_desc_2.name = "buffer_desc_2";
        EXPECT_NE(buffer_desc_1, buffer_desc_2);
        EXPECT_NE(utils::hash(buffer_desc_1), utils::hash(buffer_desc_2));

        buffer_desc_2 = buffer_desc_1;
        EXPECT_EQ(buffer_desc_1, buffer_desc_2);
        EXPECT_EQ(utils::hash(buffer_desc_1), utils::hash(buffer_desc_2));
    }

    {
        TextureDesc
            tex_desc_1 = {},
            tex_desc_2 = {
                .name        = "tex_desc_1",
                .width       = 32,
                .height      = 32,
                .depth       = 1,
                .array_size  = 1,
                .format      = Format::R8G8B8A8_UNORM,
                .mip_levels  = 1,
                .clear_value = ClearColor{1, 0, 0, 1},
                .usages      = TextureUsageFlags::RenderTarget,
            };
        EXPECT_NE(tex_desc_1, tex_desc_2);
        EXPECT_NE(utils::hash(tex_desc_1), utils::hash(tex_desc_2));

        tex_desc_2 = tex_desc_1;
        EXPECT_EQ(tex_desc_1, tex_desc_2);
        EXPECT_EQ(utils::hash(tex_desc_1), utils::hash(tex_desc_2));
    }

    {
        SamplerDesc sampler_desc_1 = {},
                    sampler_desc_2 = {
                        .name = "sampler_desc_1",
                    };

        EXPECT_NE(sampler_desc_1, sampler_desc_2);
        EXPECT_NE(utils::hash(sampler_desc_1), utils::hash(sampler_desc_2));

        sampler_desc_2 = sampler_desc_1;
        EXPECT_EQ(sampler_desc_1, sampler_desc_2);
        EXPECT_EQ(utils::hash(sampler_desc_1), utils::hash(sampler_desc_2));
    }
}

constexpr std::array supported_device_types = {
#ifdef _WIN32
    Device::Type::DX12,
#endif
    Device::Type::Vulkan,
};

class ReadbackTextureTest : public TestWithParam<Device::Type> {
protected:
    ReadbackTextureTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          device(create_device(GetParam(), test_name)) {}

    void SetUp() override {
        ASSERT_TRUE(device) << "Failed to create device";
    }

    std::pmr::string        test_name;
    std::unique_ptr<Device> device;
};

INSTANTIATE_TEST_SUITE_P(
    ReadbackTextureTest,
    ReadbackTextureTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(ReadbackTextureTest, ReadbackGradientTexture) {
    constexpr std::uint32_t width  = 64;
    constexpr std::uint32_t height = 64;

    std::vector<R8G8B8A8Unorm> pixels(width * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            auto r = static_cast<std::uint8_t>(x * 255 / (width - 1));
            auto g = static_cast<std::uint8_t>(y * 255 / (height - 1));
            auto b = static_cast<std::uint8_t>(128);
            pixels[y * width + x] = R8G8B8A8Unorm(r, g, b, 0xFF);
        }
    }

    auto texture = device->CreateTexture(
        {
            .name   = std::pmr::string(std::format("{}_tex", test_name)),
            .width  = width,
            .height = height,
            .format = Format::R8G8B8A8_UNORM,
            .usages = TextureUsageFlags::SRV | TextureUsageFlags::CopySrc | TextureUsageFlags::CopyDst,
        },
        {reinterpret_cast<const std::byte*>(pixels.data()), pixels.size() * sizeof(R8G8B8A8Unorm)});
    ASSERT_TRUE(texture);

    auto result = readback_texture(*device, *texture);
    ASSERT_EQ(result.GetDataSize(), pixels.size() * sizeof(R8G8B8A8Unorm));

    auto readback = result.Span<const R8G8B8A8Unorm>();
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            auto  i        = y * width + x;
            auto& expected = pixels[i];
            EXPECT_EQ(readback[i][0], expected[0]) << "R mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(readback[i][1], expected[1]) << "G mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(readback[i][2], expected[2]) << "B mismatch at (" << x << "," << y << ")";
            EXPECT_EQ(readback[i][3], expected[3]) << "A mismatch at (" << x << "," << y << ")";
        }
    }

    const auto output_path = std::filesystem::path("temp") /
                             std::format("ReadbackTextureTest.ReadbackGradientTexture_{}.png", magic_enum::enum_name(GetParam()));
    std::filesystem::create_directories(output_path.parent_path());

    hitagi::asset::Texture image(width, height, Format::R8G8B8A8_UNORM, result);
    ASSERT_TRUE(hitagi::asset::PngEncoder{}.Encode(image, output_path));
}
