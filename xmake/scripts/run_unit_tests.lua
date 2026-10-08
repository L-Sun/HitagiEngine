import("core.base.option")
import("async.runjobs")
import("private.action.run.runenvs")
import("devel.debugger")

-- Usage: xmake r unit_tests --jobs=8 [--verbose] [--gtest_filter=...]
-- --jobs=1 (the default) preserves the normal single-process run/debug behavior.

-- Keep report files distinct while preserving the requested format and directory.
function _shard_output(output, index, rundir)
    local format, filename = output:match("^(%w+):(.*)$")
    format = format or output
    assert(format == "xml" or format == "json", "unsupported GoogleTest output format: " .. format)
    if not filename or filename == "" then
        filename = "test_detail." .. format
    elseif filename:match("[/\\]$") then
        filename = path.join(filename, "test_detail." .. format)
    end
    filename = path.absolute(filename, rundir)
    local extension = path.extension(filename)
    local stem = extension == "" and filename or filename:sub(1, -#extension - 1)
    return format .. ":" .. stem .. ".shard-" .. index .. extension
end

function main(target)
    local args = {}
    local jobs = option.get("jobs") or "1"
    local input = table.wrap(option.get("arguments") or target:get("runargs"))
    local inspect_only = false
    local i = 1
    while i <= #input do
        local arg = input[i]
        if arg == "--jobs" or arg == "-j" then
            i = i + 1
            jobs = assert(input[i], "--jobs requires a positive integer")
        elseif arg:startswith("--jobs=") then
            jobs = arg:sub(8)
        elseif arg:match("^-j%d+$") then
            jobs = arg:sub(3)
        else
            table.insert(args, arg)
            if arg == "--gtest_list_tests" or arg == "--help" or arg == "-h" then
                inspect_only = true
            end
        end
        i = i + 1
    end
    jobs = tonumber(jobs)
    assert(jobs and jobs >= 1 and jobs == math.floor(jobs) and jobs < math.huge,
           "--jobs requires a positive integer")
    if inspect_only then
        jobs = 1
    end

    local targetfile = path.absolute(target:targetfile())
    local rundir = path.absolute(option.get("workdir") or target:rundir() or os.projectdir())
    local addenvs, setenvs = runenvs.make(target)
    if jobs == 1 then
        if option.get("debug") then
            debugger.run(targetfile, args, {curdir = rundir, addenvs = addenvs, setenvs = setenvs})
        else
            os.execv(targetfile, args, {curdir = rundir, addenvs = addenvs, setenvs = setenvs,
                                      detach = option.get("detach")})
        end
        return
    end

    assert(not option.get("debug") and not option.get("detach"),
           "parallel unit tests cannot use --debug or --detach; use --jobs=1")
    assert(not os.getenv("GTEST_TOTAL_SHARDS") and not os.getenv("GTEST_SHARD_INDEX"),
           "--jobs cannot be combined with external GoogleTest sharding")

    local output = os.getenv("GTEST_OUTPUT")
    local shard_args = {}
    for _, arg in ipairs(args) do
        if arg:startswith("--gtest_output=") then
            output = arg:sub(16)
        else
            table.insert(shard_args, arg)
        end
    end

    local logdir = path.join(os.projectdir(), "temp", "unit_tests", path.filename(os.tmpfile()))
    os.mkdir(logdir)
    print("Running unit_tests in %d parallel shards. Logs: %s", jobs, logdir)
    local started = os.mclock()
    local results = {}
    runjobs("unit_tests", function (index)
        local sharddir = path.join(logdir, "shard-" .. index)
        os.mkdir(sharddir)
        local envs = table.clone(setenvs)
        envs.GTEST_TOTAL_SHARDS = tostring(jobs)
        envs.GTEST_SHARD_INDEX = tostring(index - 1)
        envs.TEMP = sharddir
        envs.TMP = sharddir
        envs.TMPDIR = sharddir
        local argv = table.clone(shard_args)
        local report = "xml:" .. path.join(sharddir, "results.xml")
        if output then
            report = output == "" and "" or _shard_output(output, index, rundir)
        end
        table.insert(argv, "--gtest_output=" .. report)
        local stdout = path.join(sharddir, "stdout.log")
        local stderr = path.join(sharddir, "stderr.log")
        local shard_started = os.mclock()
        local code, errors = os.execv(targetfile, argv, {
            curdir = rundir, addenvs = addenvs, setenvs = envs,
            stdout = stdout, stderr = stderr, try = true
        })
        results[index] = {code = code}
        -- Print each shard as one block so concurrent GoogleTest output stays readable.
        print("\n--- Shard %d/%d (%.3fs, exit %s) ---", index, jobs,
              (os.mclock() - shard_started) / 1000, tostring(code))
        io.writefile(path.join(sharddir, "exit-code.txt"), tostring(code))
        if os.isfile(stdout) then
            io.write(io.readfile(stdout))
        end
        if os.isfile(stderr) then
            io.write(io.readfile(stderr))
        end
        if errors then
            print("Shard %d: %s", index, errors)
        end
        io.flush()
    end, {total = jobs, comax = jobs, isolate = true})

    local failed = {}
    for index = 1, jobs do
        if results[index].code ~= 0 then
            table.insert(failed, tostring(index))
        end
    end
    print("\nunit_tests: %d shards finished in %.3fs; %d failed. Logs: %s",
          jobs, (os.mclock() - started) / 1000, #failed, logdir)
    assert(#failed == 0, "unit_tests failed in shard(s): " .. table.concat(failed, ", "))
end
