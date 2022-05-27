rule("ispc")
    set_extensions(".ispc")
    add_deps("utils.inherit.links")

    -- Make sure ispc header is generated before compiling other target
    before_buildcmd_file(function (target, batchcmds, sourcefile, opt) 
        import("lib.detect.find_tool")

        local ispc = assert(find_tool("ispc"), "ispc not found!")

        local objectfile = target:objectfile(sourcefile)
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
            for _, define in pairs(target:get("defines")) do
                defines = defines .. "-D" .. define
            end
        end

        local include_path_flag = ""
        if not (target:get("includedirs") == nil) then
            include_path_flag = "-I " .. target:get("includedirs")
        end

        local pic_flag = ""
        if not target:is_plat("windows") then
            pic_flag = "--pic"
        end

        batchcmds:show_progress(opt.progress, "${color.build.object}compiling.ispc %s", sourcefile)
        batchcmds:mkdir(target:targetdir())
        batchcmds:mkdir(path.directory(objectfile))
        batchcmds:vrunv(ispc.program, {
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
        table.insert(target:objectfiles(), objectfile)

        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depmtime(os.mtime(objectfile))
        batchcmds:set_depcache(target:dependfile(objectfile))
    end)