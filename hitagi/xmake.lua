includes("math/xmake.lua")

target("engine")
    set_kind("static")
    add_files("**/*.cppm", {public = true})
    add_files("**/*.cpp")
    remove_files("test/*.cppm")
    remove_files("test/*.cpp")
    remove_files("*/test/*.cpp")
    remove_files("math/ispc/*.cpp")
    remove_files("physics/*.cppm")
    remove_files("physics/*.cpp")
    remove_files("physics/**/*.cppm")
    remove_files("physics/**/*.cpp")
    add_packages(
        "magic_enum",
        "spdlog",
        "tracy",
        "taskflow",
        "range-v3",
        "freetype",
        "imgui",
        "libpng",
        "assimp",
        "libjpeg-turbo",
        "nlohmann_json",
        "fx-gltf",
        "libsdl3",
        "vulkansdk",
        "vulkan-memory-allocator",
        "directx-shader-compiler",
        "spirv-reflect",
        {public = true}
    )
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS", {public = true})
    set_options("ispc")
    if has_config("ispc") then
        add_deps("ispc_math")
    end

    if is_plat("windows") then
        add_packages("d3d12-memory-allocator", "directx12-agility-sdk", {public = true})
        add_defines("UNICODE", "WIN32", "NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR", {public = true})
        add_syslinks("Ole32", "winmm", "imm32", "User32", {public = true})
        on_config(function (target)
            local sdk = target:pkg("directx12-agility-sdk")
            if sdk == nil then
                return
            end

            local targetdir = target:targetdir()
            if not os.exists(path.join(targetdir, "D3D12")) then
                local d3d12sdk_path = path.join(sdk:installdir(), "bin")
                os.mkdir(path.join(targetdir, "D3D12"))
                os.cp(d3d12sdk_path .. "/*", path.join(targetdir, "D3D12"))
            end
        end)
    else
        remove_files("gfx/dx12/*.cppm")
        remove_files("gfx/dx12/*.cpp")
    end

    if is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR", {public = true})
        add_syslinks("pthread", {public = true})
    end

target("unit_tests")
    set_default(false)
    set_group("test")
    set_rundir("$(projectdir)")
    add_deps("engine")
    add_includedirs("test")
    add_packages("gtest", "gmock", "benchmark", "spdlog")
    add_files("test/test_utils.cppm")
    add_files("test/unit_test_main.cpp")
    add_files("*/test/*.cpp")
    remove_files("*/test/*_test_main.cpp")
    remove_files("*/test/*_benchmark.cpp")
