#include <gtest/gtest.h>

#include "MemoryManager.hpp"
#include "ResourceManager.hpp"

namespace Hitagi {
std::unique_ptr<Core::MemoryManager>       g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager>       g_FileIOManager(new Core::FileIOManager);
std::unique_ptr<Resource::ResourceManager> g_ResourceManager(new Resource::ResourceManager);
}  // namespace Hitagi

using namespace Hitagi;

TEST(ImageParserTest, ErrorPath) {
    std::shared_ptr<Resource::Image> image;
    image = g_ResourceManager->ParseImage("Asset/Textures/a.jpg");
    EXPECT_EQ(image->GetDataSize(), 0);
    image = g_ResourceManager->ParseImage("Asset/Textures/b.ezx");
    EXPECT_EQ(image->GetDataSize(), 0);
}

TEST(ImageParserTest, Jpeg) {
    std::shared_ptr<Resource::Image> image;
    image = g_ResourceManager->ParseImage("Asset/Textures/b.jpg");
    EXPECT_TRUE(image->getData() != nullptr);
}

TEST(ImageParserTest, Tga) {
    std::shared_ptr<Resource::Image> image;
    image = g_ResourceManager->ParseImage("Asset/Textures/floor.tga");
    EXPECT_TRUE(image->getData() != nullptr);
}

TEST(ImageParserTest, png) {
    std::shared_ptr<Resource::Image> image;
    image = g_ResourceManager->ParseImage("Asset/Textures/a.png");
    EXPECT_TRUE(image->getData() != nullptr);
}

TEST(ImageParserTest, bmp) {
    std::shared_ptr<Resource::Image> image;
    image = g_ResourceManager->ParseImage("Asset/Textures/test.bmp");
    EXPECT_TRUE(image->getData() != nullptr);
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
