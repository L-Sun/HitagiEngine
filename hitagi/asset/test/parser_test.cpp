#include <hitagi/core/core.hpp>
#include <hitagi/asset/parser/bmp.hpp>
#include <hitagi/asset/parser/jpeg.hpp>
#include <hitagi/asset/parser/png.hpp>
#include <hitagi/asset/parser/tga.hpp>
#include <hitagi/asset/parser/assimp.hpp>
#include <hitagi/asset/parser/material_parser.hpp>

#include <hitagi/utils/test.hpp>

using namespace hitagi;
using namespace hitagi::core;
using namespace hitagi::math;
using namespace hitagi::asset;
using namespace hitagi::testing;

TEST(ImageParserTest, Jpeg) {
    auto parser = std::make_shared<JpegParser>();
    auto image  = parser->Parse("assets/test/test.jpg");

    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Tga) {
    auto parser = std::make_shared<TgaParser>();
    auto image  = parser->Parse("assets/test/test.tga");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Png) {
    auto parser = std::make_shared<PngParser>();
    auto image  = parser->Parse("assets/test/test.png");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Bmp) {
    auto parser = std::make_shared<BmpParser>();
    auto image  = parser->Parse("assets/test/test.bmp");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
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
    auto parser = std::make_shared<AssimpParser>();
    auto scene  = parser->Parse("assets/test/test.fbx");
    ASSERT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->camera_nodes.size(), 1);
    EXPECT_EQ(scene->instance_nodes.size(), 1);
    EXPECT_EQ(scene->light_nodes.size(), 1);
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    auto file_io_manager    = std::make_unique<core::FileIOManager>();
    hitagi::file_io_manager = file_io_manager.get();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
