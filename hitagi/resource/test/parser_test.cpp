#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi;

TEST(ImageParserTest, ErrorPath) {
    auto image = g_AssetManager->ImportImage("assets/test/azcxxcacas.jpg");
    EXPECT_TRUE(image == nullptr);
}

TEST(ImageParserTest, Jpeg) {
    auto image = g_AssetManager->ImportImage("assets/test/test.jpg");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->GetWidth(), 278);
    EXPECT_EQ(image->GetHeight(), 152);
}

TEST(ImageParserTest, Tga) {
    auto image = g_AssetManager->ImportImage("assets/test/test.tga");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->GetWidth(), 278);
    EXPECT_EQ(image->GetHeight(), 152);
}

TEST(ImageParserTest, png) {
    auto image = g_AssetManager->ImportImage("assets/test/test.png");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->GetWidth(), 278);
    EXPECT_EQ(image->GetHeight(), 152);
}

TEST(ImageParserTest, bmp) {
    auto image = g_AssetManager->ImportImage("assets/test/test.bmp");
    EXPECT_TRUE(image != nullptr);
    EXPECT_EQ(image->GetWidth(), 278);
    EXPECT_EQ(image->GetHeight(), 152);
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
