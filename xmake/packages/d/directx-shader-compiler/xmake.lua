package("directx-shader-compiler")

    set_homepage("https://github.com/microsoft/DirectXShaderCompiler/")
    set_description("DirectX Shader Compiler")
    set_license("LLVM")

    local date = {["1.5.2010"] = "2020_10-22",
                  ["1.6.2104"] = "2021_04-20",
                  ["1.6.2106"] = "2021_07_01",
                  ["1.7.2212"] = "2022_12_16",
                  ["1.7.2212+1"] = "2023_03_01"}
    if is_plat("windows") then 
        add_urls("https://github.com/microsoft/DirectXShaderCompiler/releases/download/v$(version).zip", {version = function (version) return version:gsub("%+", ".") .. "/dxc_" .. date[tostring(version)] end})
        add_versions("1.5.2010", "b691f63778f470ebeb94874426779b2f60685fc8711adf1b1f9f01535d9b67f8")
        add_versions("1.6.2104", "ee5e96d58134957443ded04be132e2e19240c534d7602e3ab8fd5adc5156014a")
        add_versions("1.6.2106", "053b2d90c227cae84e7ce636bc4f7c25acd224c31c11a324885acbf5dd8b7aac")
        add_versions("1.7.2212", "ed77c7775fcf1e117bec8b5bb4de6735af101b733d3920dda083496dceef130f")
        add_versions("1.7.2212+1", "e4e8cb7326ff7e8a791acda6dfb0cb78cc96309098bfdb0ab1e465dc29162422")
    elseif is_plat("linux") and is_arch("x86_64") then 
        add_urls("https://github.com/microsoft/DirectXShaderCompiler/releases/download/v$(version).x86_64.tar.gz", {version = function (version) return version:gsub("%+", ".") .. "/linux_dxc_" .. date[tostring(version)] end})
        add_versions("1.7.2212+1", "5d7560a8cf06dfc701b573a2effa5f3ffe4f5d4099a1951e81bbc60403952c28")
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