target("ecs_test")
    add_files("ecs_test.cpp")
    add_deps("ecs", "test_utils")
    set_group("test/ecs")

target("ecs_benchmark")
    add_files("ecs_benchmark.cpp")
    add_deps("ecs", "test_utils")
    set_group("test/ecs")