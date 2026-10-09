add_requires("usd")

target("hitagi_interop")
    set_kind("static")
    add_files("fmt.cppm", "spdlog.cppm", "vulkan.cppm", "magic_enum.cppm", "vma.cppm", "tracy.cppm", "tracy_vulkan.cppm", {public = true})
    add_files("vma.cpp")
    add_files("dxc.cppm", {public = true})
    add_files("cxxopts.cppm", {public = true})
    add_packages("cxxopts", {public = true})
    add_packages("directx-shader-compiler", {public = true})
    add_files("taskflow.cppm", "imgui.cppm", "sdl.cppm", "jolt.cppm", "freetype.cppm", "png.cppm", "jpeg.cppm", "spirv_reflect.cppm", "range_v3.cppm", {public = true})
    add_packages("taskflow", "imgui", "libsdl3", "joltphysics", "freetype", "libpng", "libjpeg-turbo", "spirv-reflect", {public = true})
    if get_config("toolchain") == "clang-cl" then
        -- clang-cl is already strict; omit range-v3's redundant /permissive-.
        add_packages("range-v3", {public = true, cxxflags = {}})
    else
        add_packages("range-v3", {public = true})
    end
    add_includedirs("..", {public = true})
    add_packages("magic_enum", "fmt", "spdlog", "vulkansdk", "vulkan-memory-allocator", {public = true})
    if has_config("profile") then
        add_packages("tracy", {public = true})
    else
        add_packages("tracy", {public = true, links = {}})
    end

    -- The SDK module and its importers must use the same Vulkan-Hpp configuration.
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS", "VULKAN_HPP_CXX_MODULE_EXPERIMENTAL_WARNING", {public = true})
    on_load(function (target)
        local sdk = target:pkg("vulkansdk")
        local includedirs = table.join(table.wrap(sdk:get("sysincludedirs")), table.wrap(sdk:get("includedirs")))
        for _, includedir in ipairs(includedirs) do
            local modulefile = path.join(includedir, "vulkan", "vulkan.cppm")
            if os.isfile(modulefile) then
                target:add("files", modulefile, {public = true})
                return
            end
        end
        raise("The Vulkan SDK must provide vulkan/vulkan.cppm (Vulkan-Hpp C++20 module)")
    end)

    if is_plat("windows") then
        add_files("dx12.cppm", "d3d12ma.cppm", "tracy_dx12.cppm", "win32.cppm", {public = true})
        add_packages("d3d12-memory-allocator", "directx12-agility-sdk", {public = true})
        add_defines("UNICODE", "WIN32", "NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR", {public = true})
        on_config(function (target)
            local sdk = target:pkg("directx12-agility-sdk")
            local targetdir = target:targetdir()
            -- Refresh the runtime when the selected Agility SDK package changes.
            local d3d12sdk_path = path.join(sdk:installdir(), "bin")
            os.mkdir(path.join(targetdir, "D3D12"))
            os.cp(d3d12sdk_path .. "/*", path.join(targetdir, "D3D12"))
        end)
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR", {public = true})
    end

-- Authoring dependencies must not become runtime dependencies of engine.
target("hitagi_interop_editor")
    set_kind("static")
    add_deps("hitagi_interop", {public = true})
    add_files("usd.cppm", "nlohmann_json.cppm", "imfilebrowser.cppm", {public = true})
    add_files("imfilebrowser.cpp")
    add_packages("usd", "nlohmann_json", {public = true})

target("hitagi_interop_test")
    set_kind("static")
    set_default(false)
    add_files("gtest.cppm", "gmock.cppm", {public = true})
    add_includedirs("..", {public = true})
    add_packages("gtest", "gmock", {public = true})

if has_config("benchmarks") then
    target("hitagi_interop_benchmark")
        set_kind("static")
        set_default(false)
        add_files("benchmark.cppm", {public = true})
        add_includedirs("..", {public = true})
        add_packages("benchmark", {public = true})
end
