#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

import std;
import core;

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    ::testing::InitGoogleTest(&argc, argv);

    auto memory_manager  = std::make_unique<hitagi::core::MemoryManager>();
    auto file_io_manager = std::make_unique<hitagi::core::FileIOManager>();

    return RUN_ALL_TESTS();
}
