rule("inject_env")
    before_run(function (target) 
        if target:get("group"):sub(1, 4) == "test" then
            target:add("runenvs", "TRACY_NO_INVARIANT_CHECK", "1")
        end
    end)