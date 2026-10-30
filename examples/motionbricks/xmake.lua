-- The TensorRT dependency belongs to this optional Windows demo, not the viewer.
option("motionbricks")
    set_default(false)
    set_description("Build the NVIDIA MotionBricks runtime demo (Windows/CUDA 12).")
option_end()

if is_plat("windows") and has_config("motionbricks") then
    add_requires("tensorrt 11.4.0", {system = false, configs = {system_cuda = true}})
    target("motionbricks-runtime")
        set_kind("static")
        set_default(false)
        add_deps("engine")
        add_packages("tensorrt", {public = true})
        add_files("../../hitagi/interop/tensorrt.cppm", "runtime.cppm", {public = true})
    target("motionbricks-demo")
        set_kind("binary")
        set_default(false)
        add_deps("motionbricks-runtime", "usd-viewer-lib", "usd-viewer-ui")
        add_files("main.cpp")
        add_files("recording.cppm", {public = true})
        set_rundir("$(projectdir)")
end
