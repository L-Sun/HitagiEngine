rule("copy-dll")
    after_link(function (target)
        if target:is_plat("windows") then
            for pkg in pairs(target:pkgs()) do
                if target:pkg(pkg):has_shared() then
                    for _, lib in ipairs(target:pkg(pkg):libraryfiles()) do
                        local name = path.filename(lib)
                        if lib:sub(-3) == "dll" and not os.exists(path.join(target:targetdir(), name)) then
                            os.cp(lib, target:targetdir())
                        end
                    end
                end
            end
        end
    end)