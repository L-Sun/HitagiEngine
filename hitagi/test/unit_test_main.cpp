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

    // MemoryManager is the only process-wide service: it installs the PMR default
    // resource. Everything else (FileIOManager, JobSystem, ...) is owned by the
    // fixtures that need it, so each test states its dependencies explicitly.
    auto memory_manager = std::make_unique<hitagi::core::MemoryManager>();

    const auto result = RUN_ALL_TESTS();

    hitagi::asset::Texture::DestroyDefaultTexture();

    return result;
}
