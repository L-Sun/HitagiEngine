-- Project assets are deliberately separate from installing the third-party SDK.
rule("ui")
    on_config(function (target)
        import("core.base.json")
        import("lib.detect.find_tool")
        local bun = assert(find_tool("bun", {version = true}), "PocketJS UI compilation requires Bun on PATH")
        local sdk = path.join(target:pkg("pocketjs"):installdir(), "share/pocketjs")
        -- All UI targets share one SDK. Generate editor metadata during configure,
        -- after package resolution, without compiling a bundle or the native engine.
        -- The SDK config is JSONC. Use its installed TypeScript parser rather than
        -- stripping comments, and preserve all upstream aliases when adding ours.
        local output = os.iorunv(bun.program, {"-e", [[
            const ts = require("typescript");
            const result = ts.readConfigFile("tsconfig.json", ts.sys.readFile);
            if (result.error) throw new Error(ts.flattenDiagnosticMessageText(result.error.messageText, "\n"));
            process.stdout.write(JSON.stringify(result.config.compilerOptions.paths));
        ]]}, {curdir = sdk})
        local paths = json.decode(output)
        paths["solid-js"] = {"node_modules/solid-js/types/index.d.ts"}
        paths["solid-js/jsx-runtime"] = {"node_modules/solid-js/jsx-runtime.d.ts"}
        paths["@pocketjs/framework/host"] = {"framework/src/host.ts"}
        paths["@pocketjs/framework/overlay-host"] = {"framework/src/overlay-host.ts"}
        local tsconfig = path.join(os.projectdir(), "build/pocketjs/tsconfig.json")
        local configtext = json.encode({
            extends = path.join(sdk, "tsconfig.json"),
            compilerOptions = {
                baseUrl = sdk,
                paths = paths,
                typeRoots = {path.join(sdk, "node_modules/@types")}
            }
        }, {pretty = true, orderkeys = true}) .. "\n"
        if not os.isfile(tsconfig) or io.readfile(tsconfig) ~= configtext then
            os.mkdir(path.directory(tsconfig))
            io.writefile(tsconfig, configtext)
        end
    end)

    on_build(function (target, opt)
        import("core.base.json")
        import("core.project.depend")
        import("lib.detect.find_tool")
        import("utils.progress")
        local bun = assert(find_tool("bun", {version = true}), "PocketJS UI compilation requires Bun on PATH")
        local sdk = path.join(target:pkg("pocketjs"):installdir(), "share/pocketjs")
        local entry = path.absolute(target:values("pocketjs.entry"), os.projectdir())
        -- Runtime defaults currently use this project-relative resource directory.
        local outdir = path.join(os.projectdir(), "build/pocketjs/ui")
        local workdir = path.join(os.projectdir(), "build/pocketjs/work", target:name())
        local name = path.basename(entry)
        local javascript = path.join(outdir, name .. ".js")
        local pak = path.join(outdir, name .. ".pak")
        local inputsfile = path.join(workdir, "inputs.json")
        local dependfile = path.join(workdir, "build.d")
        local args = {path.join(sdk, "tools/build.ts"), entry, "--no-config", "--strict-classes",
            "--density=" .. tostring(target:values("pocketjs.density") or 1),
            "--extra-chars=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./: -+()[]",
            "--outdir=" .. outdir, "--inputs-file=" .. inputsfile}
        local values = table.join({bun.program, bun.version or "", sdk}, args)
        local info = depend.load(dependfile) or {}
        if os.isfile(javascript) and os.isfile(pak) and os.isfile(inputsfile)
            and not target:is_rebuilt()
            and not depend.is_changed(info, {lastmtime = os.mtime(dependfile), values = values}) then
            return
        end
        os.mkdir(workdir)
        os.mkdir(outdir)
        -- A failed build must not make partial outputs look up to date next time.
        os.tryrm(dependfile)
        progress.show(opt.progress, "${color.build.object}compiling.pocketjs %s", path.relative(entry, os.projectdir()))
        os.vrunv(bun.program, args, {curdir = sdk, envs = {POCKETJS_WORK_DIR = workdir}})
        local files = json.loadfile(inputsfile)
        table.insert(files, entry)
        table.insert(files, path.join(os.scriptdir(), "ui.lua"))
        -- The upstream manifest includes imported TSX, fonts, resource manifests,
        -- compiler modules, and directories for optional inputs not yet created.
        depend.save({files = files, values = values}, dependfile)
    end)
