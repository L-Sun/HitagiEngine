add_requires("libpng", "assimp", "libjpeg-turbo", "crossguid")

target("enum_types")
    set_kind("headeronly")
    add_headerfiles("include/hitagi/resource/enums.hpp")
    add_includedirs("include", {public = true})
    add_deps("math")
    add_packages("magic_enum", {public = true})
    
target("resource")
    set_kind("static")
    add_files("src/resource/*.cpp")
    remove_files(
        "src/resource/asset_manager.cpp",
        "src/resource/animation.cpp",
        "src/resource/animation_manager.cpp",
        "src/resource/scene_manager.cpp"
    )
    add_includedirs("include", {public = true})
    add_deps("core", "math", "utils")
    add_packages("fmt", "crossguid")
    add_packages("magic_enum", "crossguid", {public = true})

target("parser")
    set_kind("static")
    add_files("src/parser/*.cpp")
    remove_files("src/parser/bvh.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "math", "resource")
    add_packages("libpng", "assimp", "libjpeg-turbo", "crossguid")

target("asset_manager")
    set_kind("static")
    add_files("src/resource/asset_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "parser", "resource", "utils")
