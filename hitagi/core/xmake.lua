target("runtime_module_interface")
    set_kind("headeronly")
    add_headerfiles("include/hitagi/core/runtime_module.hpp")
    add_includedirs("include", {public = true})

target("memory_manager")
    set_kind("static")
    add_files("src/buffer.cpp", "src/memory_manager.cpp")
    add_includedirs("include", {public = true})
    add_packages("spdlog")

target("file_io_manager")
    set_kind("static")
    add_files("src/file_io_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("memory_manager")
    add_packages("spdlog")

target("config_manager")
    set_kind("static")
    add_files("src/config_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("file_io_manager")
    add_packages("yaml-cpp", "spdlog")

target("timer")
    set_kind("static")
    add_files("src/timer.cpp")
    add_includedirs("include", {public = true})

target("thread_manager")
    set_kind("static")
    add_files("src/thread_manager.cpp")
    add_includedirs("include", {public = true})
    add_packages("spdlog")
    if is_os("linux") then 
        add_syslinks("pthread")
    end

target("core")
    set_kind("phony")
    add_deps("memory_manager", "file_io_manager", "config_manager", "timer", "thread_manager")
    add_packages("spdlog", "crossguid", {public = true})