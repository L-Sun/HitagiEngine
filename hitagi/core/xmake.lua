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
    add_deps("memory_manager", "file_io_manager", "timer", "thread_manager")