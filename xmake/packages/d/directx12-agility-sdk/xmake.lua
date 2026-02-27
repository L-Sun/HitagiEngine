package("directx12-agility-sdk")
    set_homepage("https://devblogs.microsoft.com/directx/directx12agility/")
    set_license("MICROSOFT SOFTWARE LICENSE TERMS")

    add_urls("https://globalcdn.nuget.org/packages/microsoft.direct3d.d3d12.$(version).nupkg")
    add_versions("1.619.0", "d5edab9a0c4d1b78ba6fe55b425eeef9fefba7d2a101e889d70fd21d481e6cb1")
    add_versions("1.613.2", "846c041941c7741490acd3fb152ebb0f6e712d55c39140c4bb57b4d8fab08fb4")
    add_versions("1.610.3", "3e4f2905aa9baf159c3a2b87f8d1ef6e9a31177d5f66da06c019af2254787248")
    add_versions("1.608.3", "bcb4eda5e6a91415a012c555d065890f505656e2c08ae27c94363d645c72b601")
    
    add_configs("shared", {description = "Using shared binaries.", default = true, type = "boolean", readonly = true})

    local sdk_versions = {
        ["1.619.0"] = "619",
        ["1.613.2"] = "613",
        ["1.610.3"] = "610",
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
