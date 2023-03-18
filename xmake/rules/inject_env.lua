rule("inject_env")
    before_run(function (target) 
        if target:get("group"):sub(1, 4) == "test" then
            target:add("runenvs", "TRACY_NO_INVARIANT_CHECK", "1")
            if is_plat("linux") then
                target:add("runenvs", "SDL_VIDEODRIVER", "wayland")
            end
        end
    end)