target("gfx_vulkan")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps("gfx_base", "vma_patch")
    add_packages("vulkansdk")
    add_packages(
        "libsdl3",
        "spirv-reflect",
        {public = true}
    )
    
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS")
    
    if is_plat("windows") then
        add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_defines("VK_USE_PLATFORM_WIN32_KHR")
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR")
    end

-- https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/pull/514
target("vma_patch")
    set_kind("static")
    add_packages("vulkan-memory-allocator", {public = true})