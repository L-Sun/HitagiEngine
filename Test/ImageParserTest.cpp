#include <gtest/gtest.h>

#include "MemoryManager.hpp"
#include "AssetManager.hpp"

using namespace Hitagi;

TEST(ImageParserTest, ErrorPath) {
    auto image = g_AssetManager->ImportImage("Asset/Textures/a.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Jpeg) {
    auto image = g_AssetManager->ImportImage("Asset/Textures/avatar.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Tga) {
    auto image = g_AssetManager->ImportImage("Asset/Textures/avatar.tga");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, png) {
    auto image = g_AssetManager->ImportImage("Asset/Textures/avatar.png");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, bmp) {
    auto image = g_AssetManager->ImportImage("Asset/Textures/test.bmp");
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
