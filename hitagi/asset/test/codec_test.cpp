#include "test_macros.hpp"
#include <spdlog/spdlog.h>

import asset;
import core;
import gfx;
import math;
import utils;
import test_utils;

using namespace hitagi;
using namespace hitagi::core;
using namespace hitagi::math;
using namespace hitagi::asset;
using namespace hitagi::testing;

TEST(ImageDecoderTest, Jpeg) {
    auto decoder = std::make_shared<JpegDecoder>();
    auto image   = decoder->Decode("assets/test/test.jpg");

    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageDecoderTest, Tga) {
    auto decoder = std::make_shared<TgaDecoder>();
    auto image   = decoder->Decode("assets/test/test.tga");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageDecoderTest, Png) {
    auto decoder = std::make_shared<PngDecoder>();
    auto image   = decoder->Decode("assets/test/test.png");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageDecoderTest, Bmp) {
    auto decoder = std::make_shared<BmpDecoder>();
    auto image   = decoder->Decode("assets/test/test.bmp");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageEncoderTest, PngRoundTrip) {
    auto decoder = std::make_shared<PngDecoder>();
    auto encoder = std::make_shared<PngEncoder>();

    auto original = decoder->Decode("assets/test/test.png");
    ASSERT_TRUE(original);

    auto encoded = encoder->Encode(*original);
    ASSERT_FALSE(encoded.Empty());

    auto decoded = decoder->Decode(encoded);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->Width(), original->Width());
    EXPECT_EQ(decoded->Height(), original->Height());
}

TEST(AssetManagerTest, AsyncTextureImportUsesJobSystem) {
    AssetManager assets("assets");

    auto job     = assets.ImportTextureAsync("assets/test/test.png");
    auto texture = job.Get();

    ASSERT_TRUE(texture);
    EXPECT_EQ(texture->Width(), 278);
    EXPECT_EQ(texture->Height(), 152);
}

TEST(AssetManagerTest, AsyncTextureImportCanBeCanceledBeforeStart) {
    AssetManager assets("assets");

    AssetManager::AssetLoadToken token;
    token.RequestCancel();

    auto job = assets.ImportTextureAsync("assets/test/test.png", token);

    EXPECT_TRUE(job.IsCancellationRequested());
    EXPECT_EQ(job.Get(), nullptr);
}
