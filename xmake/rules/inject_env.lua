rule("inject_env")
    before_run(function (target) 
        local group_name = target:get("group")
        if group_name ~= nil and group_name:sub(1, 4) == "test" then
            target:add("runenvs", "TRACY_NO_INVARIANT_CHECK", "1")
            if is_plat("linux") then
                target:add("runenvs", "SDL_VIDEODRIVER", "wayland")
            end
        end
    end)