package("directx12-agility-sdk")
    set_homepage("https://devblogs.microsoft.com/directx/directx12agility/")
    set_license("MICROSOFT SOFTWARE LICENSE TERMS")

    add_urls("https://globalcdn.nuget.org/packages/microsoft.direct3d.d3d12.$(version).nupkg")
    add_versions("1.608.3", "bcb4eda5e6a91415a012c555d065890f505656e2c08ae27c94363d645c72b601")
    
    add_configs("shared", {description = "Using shared binaries.", default = true, type = "boolean", readonly = true})

    local sdk_versions = {
        ["1.608.3"] = "608"
    }
    

    on_install("windows", function (package)
        import("utils.archive")
        local zip_file = path.filename(package:originfile()) .. ".zip"
        os.cp(package:originfile(), zip_file)
        archive.extract(zip_file, "./")
        os.rm(zip_file)

        os.cp("build/native/include", package:installdir())
        os.cp("build/native/bin/$(arch)/*", package:installdir("bin"))
        package:add("defines", "D3D12SDK_VERSION=" .. sdk_versions[tostring(package:version())])
        package:add("syslinks", "d3d12", "dxgi", "dxguid")
    end)
