#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>

#include <hitagi/utils/test.hpp>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::math;
using namespace hitagi::testing;

TEST(ImageParserTest, ErrorPath) {
    auto image = asset_manager->ImportTexture("assets/test/azcxxcacas.jpg");
    EXPECT_TRUE(!image);
}

TEST(ImageParserTest, Jpeg) {
    auto image = asset_manager->ImportTexture("assets/test/test.jpg");
    EXPECT_TRUE(image);
    EXPECT_EQ(image->width, 278);
    EXPECT_EQ(image->height, 152);
}

TEST(ImageParserTest, Tga) {
    auto image = asset_manager->ImportTexture("assets/test/test.tga");
    EXPECT_TRUE(image);
    EXPECT_EQ(image->width, 278);
    EXPECT_EQ(image->height, 152);
}

TEST(ImageParserTest, Png) {
    auto image = asset_manager->ImportTexture("assets/test/test.png");
    EXPECT_TRUE(image);
    EXPECT_EQ(image->width, 278);
    EXPECT_EQ(image->height, 152);
}

TEST(ImageParserTest, Bmp) {
    auto image = asset_manager->ImportTexture("assets/test/test.bmp");
    EXPECT_TRUE(image);
    EXPECT_EQ(image->width, 278);
    EXPECT_EQ(image->height, 152);
}

TEST(SceneParserTest, Fbx) {
    auto scene = asset_manager->ImportScene("assets/test/test.fbx");
    EXPECT_TRUE(scene != nullptr);
    EXPECT_EQ(scene->camera_nodes.size(), 1);
    EXPECT_EQ(scene->instance_nodes.size(), 1);
    EXPECT_EQ(scene->light_nodes.size(), 1);
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    auto file_io_manager = std::make_unique<core::FileIOManager>();
    auto config_manager  = std::make_unique<core::ConfigManager>();
    auto asset_manager   = std::make_unique<AssetManager>();

    hitagi::file_io_manager = file_io_manager.get();
    hitagi::config_manager  = config_manager.get();
    hitagi::asset_manager   = asset_manager.get();

    file_io_manager->Initialize();
    config_manager->Initialize();
    asset_manager->Initialize();

    ::testing::InitGoogleTest(&argc, argv);
    int test_result = RUN_ALL_TESTS();

    asset_manager->Finalize();
    config_manager->Finalize();
    file_io_manager->Finalize();

    return test_result;
}
