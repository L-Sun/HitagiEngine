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

class ImageDecoderTest : public ::testing::Test {
protected:
    FileIOManager file_io;
};

TEST_F(ImageDecoderTest, Jpeg) {
    auto decoder = std::make_shared<JpegDecoder>();
    auto image   = decoder->Decode(file_io.SyncOpenAndReadBinary("assets/test/test.jpg"));

    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST_F(ImageDecoderTest, Tga) {
    auto decoder = std::make_shared<TgaDecoder>();
    auto image   = decoder->Decode(file_io.SyncOpenAndReadBinary("assets/test/test.tga"));
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST_F(ImageDecoderTest, Png) {
    auto decoder = std::make_shared<PngDecoder>();
    auto image   = decoder->Decode(file_io.SyncOpenAndReadBinary("assets/test/test.png"));
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST_F(ImageDecoderTest, Bmp) {
    auto decoder = std::make_shared<BmpDecoder>();
    auto image   = decoder->Decode(file_io.SyncOpenAndReadBinary("assets/test/test.bmp"));
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

using ImageEncoderTest = ImageDecoderTest;

TEST_F(ImageEncoderTest, PngRoundTrip) {
    auto decoder = std::make_shared<PngDecoder>();
    auto encoder = std::make_shared<PngEncoder>();

    auto original = decoder->Decode(file_io.SyncOpenAndReadBinary("assets/test/test.png"));
    ASSERT_TRUE(original);

    auto encoded = encoder->Encode(*original);
    ASSERT_FALSE(encoded.Empty());

    auto decoded = decoder->Decode(encoded);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->Width(), original->Width());
    EXPECT_EQ(decoded->Height(), original->Height());
}

class AssetManagerTest : public ::testing::Test {
protected:
    // Declaration order matters: the manager holds references to both services.
    FileIOManager file_io;
    JobSystem     job_system;
    AssetManager  assets{file_io, job_system, "assets"};
};

TEST_F(AssetManagerTest, AsyncTextureImportUsesJobSystem) {
    auto job     = assets.ImportTextureAsync("assets/test/test.png");
    auto texture = job.Get();

    ASSERT_TRUE(texture);
    EXPECT_EQ(texture->Width(), 278);
    EXPECT_EQ(texture->Height(), 152);
}

TEST_F(AssetManagerTest, AsyncTextureImportCanBeCanceledBeforeStart) {
    AssetManager::AssetLoadToken token;
    token.RequestCancel();

    auto job = assets.ImportTextureAsync("assets/test/test.png", token);

    EXPECT_TRUE(job.IsCancellationRequested());
    EXPECT_EQ(job.Get(), nullptr);
}
