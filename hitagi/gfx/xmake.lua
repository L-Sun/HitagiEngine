add_requires("d3d12-memory-allocator", {optional = true})
add_requires("vulkan", "vulkan-memory-allocator")
add_requires("libxcb", {optional = true})

target("dx12-device")
    -- TODO try use dll
    set_kind("static")
    add_files("src/dx12/*.cpp")
    add_includedirs("include")
    add_deps("core", "math", "utils")
    add_syslinks("d3d12", "dxgi", "dxguid")
    add_packages("directxshadercompiler")
    add_packages("d3d12-memory-allocator", {public = true})
    add_defines("NOMINMAX", {public = true})

    if not is_plat("windows") then 
        set_enabled(false)
    end

target("vulkan-device")
    set_kind("static")
    add_files("src/vulkan/*.cpp")
    add_includedirs("include")
    add_deps("core", "math", "utils")
    add_packages("vulkan", "vulkan-memory-allocator", {public = true})
    
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS")
    if is_plat("windows") then
        add_defines("NOMINMAX", {public = true})
        add_defines("VK_USE_PLATFORM_WIN32_KHR")
    elseif is_plat("linux") then
        add_packages("libxcb")
        add_defines("VK_USE_PLATFORM_XCB_KHR")
    end


target("gfx")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cpp")
    add_deps("core", "math", "app", {public = true})
    add_deps("vulkan-device")
    if is_plat("windows") then
        add_deps("dx12-device")
    end
    add_packages("taskflow", {public = true})