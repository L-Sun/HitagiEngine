rule("copy-dll")
    after_link(function (target, opt)
        import("utils.progress")
        if target:is_plat("windows") then
            for pkg in pairs(target:pkgs()) do
                if target:pkg(pkg):has_shared() then
                    for _, lib in ipairs(target:pkg(pkg):libraryfiles()) do
                        local name = path.filename(lib)
                        if lib:sub(-3) == "dll" and not os.exists(path.join(target:targetdir(), name)) then
                            os.cp(lib, target:targetdir())
                            progress.show(opt.progress, "${color.build.target}copy-dll %s", name)
                        end
                    end
                end
            end
        end
    end)