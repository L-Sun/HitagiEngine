package("directx-shader-compiler")

    set_homepage("https://github.com/microsoft/DirectXShaderCompiler/")
    set_description("DirectX Shader Compiler")
    set_license("LLVM")

    -- The first value is the default date; platform keys override the archive filename.
    local releases = {
        ["1.5.2010"]   = {"2020_10-22"},
        ["1.6.2104"]   = {"2021_04-20"},
        ["1.6.2106"]   = {"2021_07_01"},
        ["1.7.2212"]   = {"2022_12_16"},
        ["1.7.2212+1"] = {"2023_03_01"},
        ["1.8.2403+2"] = {"2024_03_29"},
        ["1.9.2602"]   = {"2026_02_20"},
        ["1.9.2609"]   = {"2026_09_29", linux = "linux_dxc_2026_09_28.x86_x64.tar.gz"},
    }

    local function map_version(version)
        local release = releases[tostring(version)]
        local windows = is_plat("windows")
        local date = release[1]
        local archive = release[windows and "windows" or "linux"]
            or (windows and ("dxc_" .. date .. ".zip") or ("linux_dxc_" .. date .. ".x86_64.tar.gz"))
        return version:gsub("%+", ".") .. "/" .. archive
    end

    if is_plat("windows") then 
        add_urls("https://github.com/microsoft/DirectXShaderCompiler/releases/download/v$(version)", {version = map_version})
        add_versions("1.5.2010", "b691f63778f470ebeb94874426779b2f60685fc8711adf1b1f9f01535d9b67f8")
        add_versions("1.6.2104", "ee5e96d58134957443ded04be132e2e19240c534d7602e3ab8fd5adc5156014a")
        add_versions("1.6.2106", "053b2d90c227cae84e7ce636bc4f7c25acd224c31c11a324885acbf5dd8b7aac")
        add_versions("1.7.2212", "ed77c7775fcf1e117bec8b5bb4de6735af101b733d3920dda083496dceef130f")
        add_versions("1.7.2212+1", "e4e8cb7326ff7e8a791acda6dfb0cb78cc96309098bfdb0ab1e465dc29162422")
        add_versions("1.8.2403+2", "74874e9741f027d4321263af58d24ae0f6dde2351230680b151c87144cc0a02a")
        add_versions("1.9.2602", "a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e")
        add_versions("1.9.2609", "ad31b1fc8443175d204f77a611fdb3ef2ec42759bdc2f1167368de24a4a7e7f1")
    elseif is_plat("linux") and is_arch("x86_64") then 
        add_urls("https://github.com/microsoft/DirectXShaderCompiler/releases/download/v$(version)", {version = map_version})
        add_versions("1.7.2212+1", "5d7560a8cf06dfc701b573a2effa5f3ffe4f5d4099a1951e81bbc60403952c28")
        add_versions("1.8.2403+2", "26051824ec198854b41a481e7040ad295200774616d45698019a05b9f9cf32df")
        add_versions("1.9.2602", "a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60")
        add_versions("1.9.2609", "96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1")
    end

    add_configs("shared", {description = "Using shared binaries.", default = true, type = "boolean", readonly = true})

    on_install("windows|x64", function (package)
        os.cp("bin/x64/*", package:installdir("bin"))
        os.cp("inc/*", package:installdir("include/dxc"))
        os.cp("lib/x64/*", package:installdir("lib"))
        package:addenv("PATH", "bin")
    end)

    on_install("linux|x86_64", function (package)
        os.cp("bin/*", package:installdir("bin"))
        os.cp("include/*", package:installdir("include"))
        os.cp("lib/*", package:installdir("lib"))
        
        os.vrunv("chmod", {"+x", path.join(package:installdir("bin"), "dxc")})
        if package:has_tool("cxx", "clang") then
            package:add("cxxflags", "-fms-extensions")
            package:add("defines", "__EMULATE_UUID")
        end
        package:addenv("PATH", "bin")
    end)


    on_test(function (package)
        os.vrun("dxc -help")
        if package:is_plat("windows") then
            assert(package:has_cxxfuncs("DxcCreateInstance", {includes = {"windows.h", "dxcapi.h"}}))
        elseif package:is_plat("linux") then
            assert(package:has_cxxfuncs("DxcCreateInstance", {includes = {"dxc/dxcapi.h"}}))
        end
    end)
