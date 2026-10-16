for name, entry in pairs({
    ["editor-ui"] = "examples/editor/ui/hitagi-ui.tsx",
    ["input-fixture"] = "examples/editor/ui/input-fixture.tsx",
    ["ime-fixture"] = "examples/editor/ui/ime-fixture.tsx"
}) do
    target("pocketjs-" .. name)
        set_kind("phony")
        set_default(true)
        add_packages("pocketjs")
        add_rules("@pocketjs/ui")
        set_values("pocketjs.entry", entry)
        set_values("pocketjs.density", name == "editor-ui" and 2 or 1)
end
