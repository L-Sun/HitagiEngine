rule("ispc")
    set_extensions(".ispc")
    add_deps("utils.inherit.links")

    on_load(function (target) 
        os.runv("ispc", {"--version"})
    end)

    on_build_file(function (target, sourcefile, opt) 
        import("utils.progress")

        os.mkdir(target:targetdir())

        local objectfile = target:objectfile(sourcefile)
        os.mkdir(path.directory(objectfile))
        local headerfile = path.join(path.directory(sourcefile), path.basename(sourcefile) .. "_ispc.h")

        local enable_debug_info = ""
        if target:get("symbols") == "debug"  then 
            enable_debug_info = "-g"
        end

        local optimization = ""
        if target:get("optimize") == "none" then
            optimization = "-O0" 
        elseif target:get("optimize") == "fast" then
            optimization = "-O2"
        elseif target:get("optimize") == "faster" or target:get("optimize") == "fastest" then
            optimization = "-O3"
        elseif target:get("optimize") == "smallest" then
            optimization = "-O1"
        end

        local warnings = ""
        if target:get("warnings") == "none" then
            warnings = "--woff"
        elseif target:get("warnings") == "error" then
            warnings = "--werror"
        end
        
        local defines = ""
        if not (target:get("defines") == nil) then
            defines = target:get("defines")
        end

        local include_path_flag = ""
        if not (target:get("includedirs") == nil) then
            include_path_flag = "-I " .. target:get("includedirs")
        end

        local pic_flag = ""
        if not target:is_plat("windows") then
            pic_flag = "--pic"
        end

        os.vrunv("ispc", {
            sourcefile,
            enable_debug_info,
            pic_flag,
            defines,
            include_path_flag,
            optimization,
            "--target=host",
            "-h", headerfile,
            "-o", objectfile
        })
        progress.show(opt.progress, "${color.build.object}compiling.ispc %s", sourcefile)
        table.insert(target:objectfiles(), objectfile)
    end)


target("ispc_math")
    set_kind("static")
    add_rules("ispc")
    add_files("*.ispc")
    add_includedirs("./", {interface = true})