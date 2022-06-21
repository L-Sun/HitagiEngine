rule("clang-msvc")
    on_config(function (target)
        import("core.project.config")

        if target:is_plat("windows") then
            local cxx_compiler = config.get("cxx")
            local c_compiler = config.get("c")
            
            if cxx_compiler == "clang++" or c_compiler == "clang" then
                if is_mode("debug") then
                    target:add("syslinks", "libcmtd")
                else
                    target:add("syslinks", "libcmt")
                end
            end

            if compiler == "clang-cl" then
                target:add("cxxflags", "/EHa")
            end
        end
    end)
