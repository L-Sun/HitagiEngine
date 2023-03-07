rule("clang-msvc")
    on_config(function (target)
        import("core.project.config")

        if target:is_plat("windows") then
            local cxx_compiler = config.get("cxx")
            local c_compiler = config.get("c")
            
            if cxx_compiler == "clang++" or c_compiler == "clang" then
                -- This flag may be enabled in newer versions
                -- https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-fms-runtime-lib
                target:add("cxxflags", "-fms-runtime-lib=dll")
            end

            if cxx_compiler == "clang-cl" then
                target:add("cxxflags", "/EHa")
            end
        end
    end)
