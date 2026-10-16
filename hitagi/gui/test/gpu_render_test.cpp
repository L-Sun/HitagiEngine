#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import gfx;
import gui;
import render;
import math;
import utils;

using namespace hitagi;

TEST(PocketGpuRenderTest, SolidTextureAlphaClipAndResourceReplacement) {
    auto                   device = gfx::create_device(gfx::Device::Type::Vulkan);
    gfx::CommandQueues     queues{*device};
    auto                   bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler    compiler{"Pocket GPU test"};
    render::GuiRenderUtils renderer{*device, *bindings, compiler};
    rg::RenderGraph        graph{*device, queues, *bindings};
    const gfx::TextureDesc target_desc{
        .name   = "gpu-ui-test",
        .width  = 64,
        .height = 64,
        .format = gfx::Format::R8G8B8A8_UNORM,
        .usages = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::CopyDst,
    };
    auto output = gfx::Texture::Create(*device, queues, *bindings, target_desc);
    for (unsigned frame = 0; frame < 2; ++frame) {
        gui::GuiDrawData data;
        data.display_size = {64, 64};
        auto image        = std::make_shared<gui::GuiTextureData>();
        image->id         = std::numeric_limits<std::uint64_t>::max() - frame;
        image->width = image->height = 2;
        image->linear                = false;
        // First frame: half-alpha green; second: half-alpha blue.
        image->pixels = core::Buffer(image->width * image->height * 4);
        {
            const auto pixels = image->pixels.Span<math::R8G8B8A8Unorm>();
            for (unsigned i = 0; i < 4; ++i) {
                pixels[i][0] = 0;
                pixels[i][1] = frame ? 0 : 255;
                pixels[i][2] = frame ? 255 : 0;
                pixels[i][3] = 128;
            }
        }
        data.textures.push_back(image);
        auto&      list = data.draw_lists.emplace_back();
        const auto quad = [&](math::Color color, std::uint64_t resource, math::vec4f clip) {
            const auto base = static_cast<std::uint32_t>(list.vertices.size());
            list.commands.push_back({.element_count = 6, .index_offset = static_cast<std::uint32_t>(list.indices.size()), .clip_rect = clip, .texture = {.resource = resource}});
            list.vertices.push_back({{0, 0}, {0, 0}, color});
            list.vertices.push_back({{64, 0}, {1, 0}, color});
            list.vertices.push_back({{64, 64}, {1, 1}, color});
            list.vertices.push_back({{0, 64}, {0, 1}, color});
            for (unsigned index : {0u, 1u, 2u, 0u, 2u, 3u}) list.indices.push_back(base + index);
        };
        quad(math::Color::Red(), 0, {0, 0, 64, 64});
        quad(math::Color::White(), image->id, {16, 16, 48, 48});
        const auto target = graph.Create(target_desc);
        renderer.GuiPass(graph, target, data, true);
        graph.QueueTextureExtraction(target, output);
        ASSERT_TRUE(graph.Compile());
        graph.Execute();
        const auto pixels = gfx::readback_texture(*device, queues, *bindings, *output);
        const auto pixel  = [&](unsigned x, unsigned y, unsigned channel) {
            return std::to_integer<unsigned>(pixels.GetData()[(y * 64 + x) * 4 + channel]);
        };
        EXPECT_EQ(pixel(8, 8, 0), 255u);  // Solid path and clipping outside the image.
        EXPECT_EQ(pixel(8, 8, 1), 0u);
        EXPECT_NEAR(pixel(32, 32, 0), 127u, 1);
        EXPECT_NEAR(pixel(32, 32, frame ? 2 : 1), 128u, 1);
        EXPECT_EQ(pixel(32, 32, 3), 255u);
        graph.ClearImportedResources();
    }
}
