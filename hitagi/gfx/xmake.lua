add_requires("d3d12-memory-allocator", {optional = true})
add_requires("vulkan", "vulkan-memory-allocator", "directx-shader-compiler", "spirv-reflect")

target("gfx-resource")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/gpu_resource.cpp")
    add_deps("core", "math", "utils", {public = true})

target("dx12-device")
    -- TODO try use dll
    set_kind("static")
    add_files("src/dx12/*.cpp")
    add_deps("gfx-resource")
    add_syslinks("d3d12", "dxgi", "dxguid")
    add_packages("directx-shader-compiler")
    add_packages("d3d12-memory-allocator", {public = true})
    add_defines("NOMINMAX", {public = true})

    if not is_plat("windows") then 
        set_enabled(false)
    end

target("vulkan-device")
    set_kind("static")
    add_files("src/vulkan/*.cpp")
    add_includedirs("include")
    add_deps("gfx-resource")
    add_packages(
        "vulkan",
        "vulkan-memory-allocator",
        "directx-shader-compiler",
        {public = true}
    )
    add_packages("spirv-reflect")
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS")
    if is_plat("windows") then
        add_defines("NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR")
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR")
    end

target("gfx_device")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/device.cpp")
    add_deps("vulkan-device")
    if is_plat("windows") then
        add_deps("dx12-device")
    end

target("gfx")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/render_graph.cpp")
    add_deps("gfx-resource", "gfx_device", {inherit = false})
    add_packages("taskflow", {public = true})