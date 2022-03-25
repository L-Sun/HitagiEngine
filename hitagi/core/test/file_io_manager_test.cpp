#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/utils/test.hpp>

#include <iostream>
#include <string>
#include <cstdio>

using namespace hitagi;

std::filesystem::path create_temp_file(std::string_view name, std::string_view content) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    path += ".tmp";

    std::fstream file;
    file.open(path, std::ios::out | std::ios::binary);
    EXPECT_TRUE(file.is_open()) << "Can not create temp file for testing!";

    file.write(content.data(), content.size());
    file.close();

    return path;
}

void remove_temp_file(const std::filesystem::path& path) {
    EXPECT_EQ(std::remove(path.string().c_str()), 0) << "Can not delete test temp file!";
}

TEST(FileIoManagerTest, ReadFile) {
    auto content =
        "Hello world!\r\n"
        "Current test is reading test.";

    auto path = create_temp_file("ReadFile", content);

    auto        buffer = g_FileIoManager->SyncOpenAndReadBinary(path);
    std::string result(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());

    EXPECT_STREQ(content, result.c_str());

    remove_temp_file(path);
}

TEST(FileIoManagerTest, SaveFile) {
    std::string  content = "Hello world!";
    core::Buffer buffer(content.data(), content.size());
    auto         path = std::filesystem::temp_directory_path() / "SaveFile.tmp";

    g_FileIoManager->SaveBuffer(buffer, path);
    buffer = g_FileIoManager->SyncOpenAndReadBinary(std::filesystem::temp_directory_path() / "Save");

    EXPECT_STREQ(content.c_str(), std::string(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize()).c_str());

    remove_temp_file(path);
}

auto main(int argc, char* argv[]) -> int {
    ::testing::InitGoogleTest(&argc, argv);

    g_MemoryManager->Initialize();
    g_FileIoManager->Initialize();

    int result = RUN_ALL_TESTS();

    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();

    return result;
}
