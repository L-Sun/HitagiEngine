add_requires("libpng", "assimp", "libjpeg-turbo", "fx-gltf")

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
    add_packages("fmt")

target("parser")
    set_kind("static")
    add_files("src/parser/*.cpp")
    remove_files(
        "src/parser/bvh.cpp",
        "src/parser/gltf.cpp"
    )
    add_includedirs("include", {public = true})
    add_deps("core", "math", "resource")
    add_packages("libpng", "assimp", "libjpeg-turbo", "nlohmann_json", "fx-gltf")

target("asset_manager")
    set_kind("static")
    add_files("src/resource/asset_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "parser", "resource", "utils")

target("scene_manager")
    set_kind("static")
    add_files("src/resource/scene_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "resource")