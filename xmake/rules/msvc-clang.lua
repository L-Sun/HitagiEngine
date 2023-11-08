rule("clang-msvc")
    on_config(function (target)
        import("core.project.config")

        if target:is_plat("windows") then
            local cxx_compiler = config.get("cxx")
            local c_compiler = config.get("c")
            
            if cxx_compiler == "clang++" or c_compiler == "clang" then
                -- This flag only be enabled in clang>=17.0.1en
                if is_mode("debug") then
                    target:add("cxxflags", "-fms-runtime-lib=dll_dbg")
                else
                    target:add("cxxflags", "-fms-runtime-lib=dll")
                end
            end

            if cxx_compiler == "clang-cl" then
                target:add("cxxflags", "/EHa")
            end
        end
    end)
