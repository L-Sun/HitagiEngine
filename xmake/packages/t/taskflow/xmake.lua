package("taskflow")
    set_kind("library", {headeronly = true})
    set_homepage("https://taskflow.github.io/")
    set_description("A C++ task-parallel programming library")
    set_license("MIT")
    add_urls("https://github.com/taskflow/taskflow/archive/refs/tags/$(version).tar.gz")
    add_versions("v4.1.0", "2107f90e315e48a676922010b036357ff2b0c6b9160ce17fa9396e5860b1d715")
    add_configs("modules", {description = "Make internal profiler helpers usable through a C++ module", default = false, type = "boolean"})

    if is_plat("linux") then
        add_syslinks("pthread")
    end

    on_install(function (package)
        if package:config("modules") then
            -- MSVC imports calls to these helpers but drops their internal-linkage
            -- definitions. External inline linkage preserves their bodies and API.
            local header = "taskflow/observer/tfprof.hpp"
            io.replace(header, "static inline void _tf_rule(", "inline void _tf_rule(", {plain = true})
            io.replace(header, "static inline double _tf_time_scale(", "inline double _tf_time_scale(", {plain = true})
        end
        os.cp("taskflow", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <taskflow/taskflow.hpp>
            void test() {
                tf::Taskflow graph;
                tf::Executor executor(2);
                graph.emplace([] {});
                executor.run(graph).get();
            }
        ]]}, {configs = {languages = "c++20"}}))
    end)
