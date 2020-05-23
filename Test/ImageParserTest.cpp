#include <gtest/gtest.h>

#include "MemoryManager.hpp"
#include "ResourceManager.hpp"

using namespace Hitagi;

TEST(ImageParserTest, ErrorPath) {
    auto image = g_ResourceManager->ParseImage("Asset/Textures/a.jpg");
    EXPECT_TRUE(image.Empty());
    image = g_ResourceManager->ParseImage("Asset/Textures/b.ezx");
    EXPECT_TRUE(image.Empty());
}

TEST(ImageParserTest, Jpeg) {
    auto image = g_ResourceManager->ParseImage("Asset/Textures/avatar.jpg");
    EXPECT_TRUE(!image.Empty());
}

TEST(ImageParserTest, Tga) {
    auto image = g_ResourceManager->ParseImage("Asset/Textures/avatar.tga");
    EXPECT_TRUE(!image.Empty());
}

TEST(ImageParserTest, png) {
    auto image = g_ResourceManager->ParseImage("Asset/Textures/avatar.png");
    EXPECT_TRUE(!image.Empty());
}

TEST(ImageParserTest, bmp) {
    auto image = g_ResourceManager->ParseImage("Asset/Textures/avatar.bmp");
    EXPECT_TRUE(!image.Empty());
}

int main(int argc, char* argv[]) {
    g_MemoryManager->Initialize();
    g_FileIOManager->Initialize();
    g_ResourceManager->Initialize();

    ::testing::InitGoogleTest(&argc, argv);
    int testResult = RUN_ALL_TESTS();

    g_ResourceManager->Finalize();
    g_FileIOManager->Finalize();
    g_MemoryManager->Finalize();

    return testResult;
}
