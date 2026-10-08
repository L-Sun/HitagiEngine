#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

import std;
import core;
import asset;

int main(int argc, char** argv) {
    // Consume our flags before passing the remaining arguments to GoogleTest.
    bool verbose        = false;
    int  remaining_argc = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else {
            argv[remaining_argc++] = argv[i];
        }
    }
    argc       = remaining_argc;
    argv[argc] = nullptr;
    spdlog::set_level(verbose ? spdlog::level::trace : spdlog::level::off);

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
