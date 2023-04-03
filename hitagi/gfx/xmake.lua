add_requires("d3d12-memory-allocator", "directx12-agility-sdk", {optional = true})
add_requires("vulkansdk", "vulkan-memory-allocator", "directx-shader-compiler", "spirv-reflect")

target("gfx_resource")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/gpu_resource.cpp")
    add_deps("core", "math", "utils", {public = true})

target("shader_compiler")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/shader_compiler.cpp")
    add_deps("gfx_resource")
    add_packages("directx-shader-compiler", {public = true})

target("dx12_device")
    -- TODO try use dll
    set_kind("static")
    add_files("src/dx12/*.cpp")
    add_deps("gfx_resource", "shader_compiler")
    add_packages("d3d12-memory-allocator", "directx12-agility-sdk", {public = true})
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    if not is_plat("windows") then 
        set_enabled(false)
    end

    on_config(function (target)
        if not os.exists(path.join(target:targetdir(), "D3D12")) then 
            local d3d12sdk_path = path.join(target:pkg("directx12-agility-sdk"):installdir(), "bin")
            os.mkdir(path.join(target:targetdir(), "D3D12"))
            os.cp(d3d12sdk_path .. "/*", path.join(target:targetdir(), "D3D12"))
        end
    end)

target("vulkan_device")
    set_kind("static")
    add_files("src/vulkan/*.cpp")
    add_includedirs("include")
    add_deps("gfx_resource", "shader_compiler")
    add_packages("vulkansdk", "vulkan-memory-allocator", {public = true})
    add_packages("spirv-reflect", "libsdl")
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS")
    if is_plat("windows") then
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR")
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR")
    end

target("gfx_device")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/device.cpp")
    add_deps("vulkan_device")
    if is_plat("windows") then
        add_deps("dx12_device")
    end

target("gfx")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/render_graph.cpp")
    add_deps("gfx_resource", "gfx_device", {inherit = false})
    add_packages("taskflow", {public = true})