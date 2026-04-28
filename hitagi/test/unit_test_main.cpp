#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

import std;
import core;
import asset;

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);

    // GoogleTest keeps some process-lifetime state; initialize it before
    // MemoryManager replaces the global PMR default resource.
    ::testing::InitGoogleTest(&argc, argv);

    auto memory_manager  = std::make_unique<hitagi::core::MemoryManager>();
    auto file_io_manager = std::make_unique<hitagi::core::FileIOManager>();
    auto job_system      = std::make_unique<hitagi::core::JobSystem>();

    const auto result = RUN_ALL_TESTS();

    hitagi::asset::Texture::DestroyDefaultTexture();

    return result;
}
