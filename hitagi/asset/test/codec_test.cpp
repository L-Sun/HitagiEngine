#include "test_macros.hpp"
#include <spdlog/spdlog.h>

import asset;
import core;
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

TEST(MaterialParserTest, JSON) {
    auto parser = std::make_shared<MaterialJSONParser>();
    auto mat    = parser->Parse("assets/test/test-mat.json");
    ASSERT_TRUE(mat);
    EXPECT_EQ(mat->GetDefaultParameters()[0].name, "diffuse");
    EXPECT_EQ(mat->GetDefaultParameters()[1].name, "ambient");
    EXPECT_EQ(mat->GetDefaultParameters()[2].name, "specular");
    EXPECT_EQ(mat->GetDefaultParameters()[3].name, "specular_power");
    EXPECT_EQ(mat->GetDefaultParameters()[4].name, "skin");

    EXPECT_TRUE(std::holds_alternative<vec4f>(mat->GetDefaultParameters()[0].value));
    EXPECT_TRUE(std::holds_alternative<vec4f>(mat->GetDefaultParameters()[1].value));
    EXPECT_TRUE(std::holds_alternative<vec4f>(mat->GetDefaultParameters()[2].value));
    EXPECT_TRUE(std::holds_alternative<float>(mat->GetDefaultParameters()[3].value));
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<Texture>>(mat->GetDefaultParameters()[4].value));

    EXPECT_VEC_EQ(std::get<vec4f>(mat->GetDefaultParameters()[0].value), vec4f(1.0, 0.8, 0.5, 1.0));
    EXPECT_VEC_EQ(std::get<vec4f>(mat->GetDefaultParameters()[1].value), vec4f(0.5, 0.8, 0.5, 1.0));
    EXPECT_VEC_EQ(std::get<vec4f>(mat->GetDefaultParameters()[2].value), vec4f(0.0, 0.8, 0.5, 1.0));
    EXPECT_NEAR(std::get<float>(mat->GetDefaultParameters()[3].value), 23.0, 1e-4);
    auto tex = std::get<std::shared_ptr<Texture>>(mat->GetDefaultParameters()[4].value);
    EXPECT_STREQ(tex->GetPath().string().c_str(), "assets/test/test.jpg");
}

TEST(SceneParserTest, Fbx) {
    utils::EnumArray<std::shared_ptr<ImageDecoder>, ImageFormat> image_decoders;
    image_decoders[ImageFormat::PNG]  = std::make_shared<PngDecoder>();
    image_decoders[ImageFormat::JPEG] = std::make_shared<JpegDecoder>();
    image_decoders[ImageFormat::TGA]  = std::make_shared<TgaDecoder>();
    image_decoders[ImageFormat::BMP]  = std::make_shared<BmpDecoder>();

    AssimpParser parser(image_decoders);
    auto         scene = parser.Parse("assets/test/test.fbx");
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->GetCameraEntities().size(), 1);
    EXPECT_EQ(scene->GetMeshEntities().size(), 1);
    EXPECT_EQ(scene->GetLightEntities().size(), 1);
}
