#include "test_macros.hpp"
#include <spdlog/spdlog.h>
#include <cstdio>

import std;
import core;
import test_utils;

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

    core::FileIOManager file_io;
    auto                buffer = file_io.SyncOpenAndReadBinary(path);
    std::pmr::string result(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());

    EXPECT_STREQ(content, result.c_str());

    remove_temp_file(path);
}

TEST(FileIoManagerTest, SaveFile) {
    std::pmr::string content = "Hello world!";
    core::Buffer     buffer(content.size(), reinterpret_cast<const std::byte*>(content.data()));
    auto             path = std::filesystem::temp_directory_path() / "SaveFile.tmp";

    core::FileIOManager file_io;
    file_io.SaveBuffer(buffer, path);
    buffer = file_io.SyncOpenAndReadBinary(path);

    EXPECT_STREQ(content.c_str(), std::pmr::string(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize()).c_str());

    remove_temp_file(path);
}
