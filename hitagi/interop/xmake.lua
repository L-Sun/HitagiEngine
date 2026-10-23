add_requires(
    "vulkan-memory-allocator",
    "directx-shader-compiler"
)
-- Use the compiler shipped with the Vulkan SDK, including its matching headers
-- and runtime DLL. DXC remains a downstream dependency for DXIL generation.
add_requires("vulkansdk", {configs = {utils = {"slang"}}})
add_requires("taskflow v4.1.0", {configs = {modules = true}})

-- Only compiled libraries that we intentionally link dynamically need this setting.
add_requires(
    "libpng",
    "libjpeg-turbo",
    "libsdl3",
    "joltphysics",
    {configs = {shared = true}}
)
add_requires("gtest", {configs = {shared = true, gmock = false}})

-- Compiled libraries avoid MSVC module errors in spdlog's header-only implementation.
-- Keep the direct and transitive fmt dependencies on the same static configuration.
add_requires("fmt", {configs = {header_only = false, shared = false}})
add_requires("magic_enum", {configs = {modules = true}})
add_requires("tracy v0.14.1", {configs = {shared = true, tracy_enable = has_config("profile")}})
add_requires("spdlog", {configs = {header_only = false, shared = false, fmt_external = true}})
add_requireconfs("spdlog.fmt", {version = "latest", override = true, configs = {header_only = false, shared = false}})
if is_plat("windows") then
    add_requires("d3d12-memory-allocator", {configs = {shared = true}})
    add_requires("directx12-agility-sdk")
end
if has_config("benchmarks") then
    add_requires("benchmark", {configs = {shared = true}})
end

add_requires("usd")
add_requires("bullet3 3.25", {configs = {shared = false}})
add_requires("pocketjs 0.14.0", {system = false})

target("hitagi_interop")
    set_kind("static")
    add_files("fmt.cppm", "spdlog.cppm", "vulkan.cppm", "magic_enum.cppm", "vma.cppm", "tracy.cppm", "tracy_vulkan.cppm", {public = true})
    add_files("vma.cpp")
    add_files("bullet.cppm", {public = true})
    add_packages("bullet3", {public = true})
    add_files("slang.cppm", {public = true})
    add_packages("directx-shader-compiler", {public = true})
    add_files("taskflow.cppm", "sdl.cppm", "jolt.cppm", "png.cppm", "jpeg.cppm", {public = true})
    add_packages("taskflow", "libsdl3", "joltphysics", "libpng", "libjpeg-turbo", {public = true})
    add_files("pocketjs.cppm", {public = true})
    add_packages("pocketjs", {public = true})
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
    add_files("usd.cppm", {public = true})
    add_packages("usd", {public = true})

target("hitagi_interop_test")
    set_kind("static")
    set_default(true)
    add_files("gtest.cppm", {public = true})
    add_includedirs("..", {public = true})
    add_packages("gtest", {public = true})

if has_config("benchmarks") then
    target("hitagi_interop_benchmark")
        set_kind("static")
        set_default(true)
        add_files("benchmark.cppm", {public = true})
        add_includedirs("..", {public = true})
        add_packages("benchmark", {public = true})
end
