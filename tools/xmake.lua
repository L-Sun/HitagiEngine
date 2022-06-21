add_requires("highfive")

target("bvh_extractor")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_deps("math", "core")
    add_packages("cxxopts", "fmt", "highfive")
