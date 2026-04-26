target("gfx_vulkan")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("vk_device.cppm", {public = true})
    add_files("*.cpp", {public = true})
    add_deps("gfx_base", "vma_patch")
    add_packages("vulkansdk", {public = true})
    add_packages(
        "libsdl3",
        "spirv-reflect",
        {public = true}
    )
    
    add_defines("VULKAN_HPP_NO_CONSTRUCTORS", {public = true})
    
    if is_plat("windows") then
        add_defines("NOMINMAX", "VK_USE_PLATFORM_WIN32_KHR", {public = true})
    elseif is_plat("linux") then
        add_defines("VK_USE_PLATFORM_WAYLAND_KHR", {public = true})
    end

-- https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/pull/514
target("vma_patch")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_packages("vulkan-memory-allocator", {public = true})
