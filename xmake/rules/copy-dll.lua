rule("copy-dll")
    before_link(function (target, opt)
        import("utils.progress")
        for pkg in pairs(target:pkgs()) do
            if target:pkg(pkg):has_shared() then
                for _, lib in ipairs(target:pkg(pkg):libraryfiles()) do
                    local name = path.filename(lib)
                    if os.exists(path.join(target:targetdir(), name)) then
                        goto continue
                    end
                    if lib:sub(-3) == "dll" then
                        os.cp(lib, target:targetdir())
                        progress.show(opt.progress, "${color.build.target}copy-dll %s", name)
                    elseif string.match(lib, ".so") then
                        os.cp(lib, target:targetdir())
                        progress.show(opt.progress, "${color.build.target}copy-so %s", name)
                    end
                    ::continue::
                end
            end
        end
    end)