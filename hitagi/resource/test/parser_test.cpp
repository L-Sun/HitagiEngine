#include <hitagi/utils/test.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>

using namespace hitagi;
using namespace hitagi::resource;

TEST(ImageParserTest, ErrorPath) {
    auto image = g_AssetManager->ImportImage("assets/test/azcxxcacas.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Jpeg) {
    auto image = g_AssetManager->ImportImage("assets/test/test.jpg");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Tga) {
    auto image = g_AssetManager->ImportImage("assets/test/test.tga");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Png) {
    auto image = g_AssetManager->ImportImage("assets/test/test.png");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(ImageParserTest, Bmp) {
    auto image = g_AssetManager->ImportImage("assets/test/test.bmp");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->Width(), 278);
    EXPECT_EQ(image->Height(), 152);
}

TEST(SceneParserTest, Fbx) {
    Scene scene;
    g_AssetManager->ImportScene(scene, "assets/test/test.fbx");
    EXPECT_EQ(scene.cameras.size(), 1);
    EXPECT_EQ(scene.geometries.size(), 1);
    EXPECT_EQ(scene.lights.size(), 1);
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();
    g_AssetManager->Initialize();

    ::testing::InitGoogleTest(&argc, argv);
    int test_result = RUN_ALL_TESTS();

    g_AssetManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

    return test_result;
}
