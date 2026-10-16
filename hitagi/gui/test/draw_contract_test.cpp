#include "test_macros.hpp"
import interop.gtest;
import interop.pocketjs;
import std;

namespace pocket = hitagi::interop::pocketjs;

static_assert(std::is_same_v<decltype(PocketTexture::format), pocket::PixelFormat>);
static_assert(sizeof(pocket::PixelFormat) == sizeof(std::uint32_t));
static_assert(alignof(pocket::PixelFormat) == alignof(std::uint32_t));

TEST(PocketDrawContractTest, RejectsUnknownAndTruncatedCommands) {
    EXPECT_EQ(pocket::CommandWordCount(nullptr, 0), 0u);
    const std::array<std::uint32_t, 1> unknown = {0xffffffff};
    EXPECT_EQ(pocket::CommandWordCount(unknown.data(), 1), 0u);
    const std::array<std::uint32_t, 4> rect = {std::to_underlying(pocket::DrawOp::Rect), 0, 0, 0};
    EXPECT_EQ(pocket::CommandWordCount(rect.data(), 3), 0u);
    EXPECT_EQ(pocket::CommandWordCount(rect.data(), 4), 4u);
}

TEST(PocketDrawContractTest, ValidatesGlyphAndPaddedTextPayloads) {
    const std::array<std::uint32_t, 7> glyphs = {std::to_underlying(pocket::DrawOp::GlyphRun), 2u << 16, 0, 0, 0, 0, 0};
    EXPECT_EQ(pocket::CommandWordCount(glyphs.data(), 1), 0u);
    EXPECT_EQ(pocket::CommandWordCount(glyphs.data(), 6), 0u);
    EXPECT_EQ(pocket::CommandWordCount(glyphs.data(), 7), 7u);
    std::array<std::uint32_t, 10> text = {std::to_underlying(pocket::DrawOp::TextRun), 0, 0, 0, 0, 0, 0, 5, 0, 0};
    EXPECT_EQ(pocket::CommandWordCount(text.data(), 9), 0u);
    EXPECT_EQ(pocket::CommandWordCount(text.data(), 10), 10u);
    text[7] = 4;
    EXPECT_EQ(pocket::CommandWordCount(text.data(), 9), 9u);
    text[7] = 0xffffffff;
    EXPECT_EQ(pocket::CommandWordCount(text.data(), 10), 0u);
}
