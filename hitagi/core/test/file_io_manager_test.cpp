#include <hitagi/utils/test.hpp>
#include <hitagi/core/file_io_manager.hpp>

#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>

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

    auto             buffer = core::FileIOManager::Get()->SyncOpenAndReadBinary(path);
    std::pmr::string result(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());

    EXPECT_STREQ(content, result.c_str());

    remove_temp_file(path);
}

TEST(FileIoManagerTest, SaveFile) {
    std::pmr::string content = "Hello world!";
    core::Buffer     buffer(content.size(), reinterpret_cast<const std::byte*>(content.data()));
    auto             path = std::filesystem::temp_directory_path() / "SaveFile.tmp";

    core::FileIOManager::Get()->SaveBuffer(buffer, path);
    buffer = core::FileIOManager::Get()->SyncOpenAndReadBinary(path);

    EXPECT_STREQ(content.c_str(), std::pmr::string(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize()).c_str());

    remove_temp_file(path);
}

auto main(int argc, char* argv[]) -> int {
    spdlog::set_level(spdlog::level::off);
    ::testing::InitGoogleTest(&argc, argv);

    auto file_io_manager = std::make_unique<core::FileIOManager>();

    return RUN_ALL_TESTS();
}
