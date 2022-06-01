rule("clang-msvc")
    on_config(function (target)
        import("core.project.config")
        

        if target:is_plat("windows") then
            local compiler = config.get("cxx")
            
            if compiler == "clang++" and is_mode("debug") then
                target:add("syslinks", "libcmtd")
            else
                target:add("syslinks", "libcmt")
            end

            if compiler == "clang-cl" then
                target:add("cxxflags", "/EHa")
            end
        end
    end)
