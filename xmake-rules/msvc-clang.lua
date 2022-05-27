rule("clang-msvc")
    on_config(function (target)
        import("core.project.config")
        local compiler = config.get("cxx")

        if target:is_plat("windows") and compiler == "clang++" then
            if is_mode("debug") then
                target:add("defines", "_Debug")
                target:add("syslinks", "libcmtd")
            else
                target:add("syslinks", "libcmt")
            end
        end
    end)
