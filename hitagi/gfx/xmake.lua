add_requires("d3d12-memory-allocator", "directxshadercompiler")
add_requires("vulkansdk")

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
    add_packages("vulkansdk", {public = true})


target("gfx")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cpp")
    add_deps("core", "math", "app", {public = true})
    add_deps("dx12-device", "vulkan-device")
    add_packages("taskflow", {public = true})