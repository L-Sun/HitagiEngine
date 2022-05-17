#include <filesystem>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi;
using namespace hitagi::core;

TEST(ConfigManagerTest, LoadConfigFile) {
    std::pmr::string yaml = R"(
title: Hitagi Engine
version: "v0.2.0"
width: 1960
height: 1280
)";

    Buffer buffer(yaml.data(), yaml.size(), 4);
    auto   path = std::filesystem::temp_directory_path() / "load_config_file_test.yaml";
    g_FileIoManager->SaveBuffer(buffer, path);

    EXPECT_NO_THROW({ g_ConfigManager->LoadConfig(path); });
    auto config = g_ConfigManager->GetConfig();
    EXPECT_STREQ(config.title.c_str(), "Hitagi Engine");
    EXPECT_STREQ(config.version.c_str(), "v0.2.0");
}

int main(int argc, char** argv) {
    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();
    g_ConfigManager->Initialize();

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    g_ConfigManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();
    return result;
}