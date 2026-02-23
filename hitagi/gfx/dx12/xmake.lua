target("gfx_dx12")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps("gfx_base")
    add_packages("d3d12-memory-allocator", "directx12-agility-sdk", {public = true})
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

    on_config(function (target)
        if not os.exists(path.join(target:targetdir(), "D3D12")) then
            local d3d12sdk_path = path.join(target:pkg("directx12-agility-sdk"):installdir(), "bin")
            os.mkdir(path.join(target:targetdir(), "D3D12"))
            os.cp(d3d12sdk_path .. "/*", path.join(target:targetdir(), "D3D12"))
        end
    end)