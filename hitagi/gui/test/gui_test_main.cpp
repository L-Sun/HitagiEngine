#include <gtest/gtest.h>

import std;
import core;

int main(int argc, char** argv) {
    auto file_io_manager = std::make_unique<hitagi::core::FileIOManager>();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
