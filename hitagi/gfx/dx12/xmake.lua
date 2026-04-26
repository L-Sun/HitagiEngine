target("gfx_dx12")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("dx12_device.cppm", {public = true})
    add_files("*.cpp")
    add_deps("gfx_base")
    add_packages("d3d12-memory-allocator", "directx12-agility-sdk", {public = true})
    add_defines("NOMINMAX", {public = true})

    on_config(function (target)
        local targetdir = target:targetdir()
        if not os.exists(path.join(targetdir, "D3D12")) then
            local d3d12sdk_path = path.join(target:pkg("directx12-agility-sdk"):installdir(), "bin")
            os.mkdir(path.join(targetdir, "D3D12"))
            os.cp(d3d12sdk_path .. "/*", path.join(targetdir, "D3D12"))
        end
    end)
