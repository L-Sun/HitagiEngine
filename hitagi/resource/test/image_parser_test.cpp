#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>

#include <gtest/gtest.h>

using namespace hitagi;

TEST(ImageParserTest, ErrorPath) {
    auto image = g_AssetManager->ImportImage("Assets/Textures/a.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Jpeg) {
    auto image = g_AssetManager->ImportImage("Assets/Textures/avatar.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Tga) {
    auto image = g_AssetManager->ImportImage("Assets/Textures/avatar.tga");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, png) {
    auto image = g_AssetManager->ImportImage("Assets/Textures/avatar.png");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, bmp) {
    auto image = g_AssetManager->ImportImage("Assets/Textures/test.bmp");
    EXPECT_TRUE(image == nullptr);
}

int main(int argc, char* argv[]) {
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
