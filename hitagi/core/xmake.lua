target("runtime_module_interface")
    set_kind("static")
    add_files("src/runtime_module.cpp")
    add_includedirs("include", {public = true})
    add_packages("spdlog", {public = true})

target("memory_manager")
    set_kind("static")
    add_files("src/buffer.cpp", "src/memory_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("runtime_module_interface")
    
target("file_io_manager")
    set_kind("static")
    add_files("src/file_io_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("memory_manager", "runtime_module_interface")
    
target("config_manager")
    set_kind("static")
    add_files("src/config_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("file_io_manager", "runtime_module_interface")
    add_packages("nlohmann_json")
    
target("timer")
    set_kind("static")
    add_files("src/timer.cpp")
    add_includedirs("include", {public = true})

target("thread_manager")
    set_kind("static")
    add_files("src/thread_manager.cpp")
    add_deps("runtime_module_interface")
    add_includedirs("include", {public = true}) 
    if is_os("linux") then 
        add_syslinks("pthread")
    end

target("core")
    set_kind("phony")
    add_deps(
        "runtime_module_interface",
        "memory_manager",
        "file_io_manager",
        "config_manager",
        "timer",
        "thread_manager"
    )
    add_includedirs("include", {public = true})
    add_packages("crossguid", {public = true})