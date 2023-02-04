add_requires("crossguid", "libpng", "assimp", "libjpeg-turbo", "fx-gltf", "usd")

target("asset")
    set_kind("static")
    add_files("src/*.cpp")
    remove_files(
        "src/asset_manager.cpp",
        "src/animation_manager.cpp"
    )
    add_includedirs("include", {public = true})
    add_deps("core", "math", "gfx", "ecs", "utils")
    add_packages("crossguid", {public = true})

target("parser")
    set_kind("static")
    add_files("src/parser/*.cpp")
    remove_files(
        "src/parser/bvh.cpp",
        "src/parser/gltf.cpp"
    )
    add_includedirs("include", {public = true})
    add_deps("core", "math", "asset")
    add_packages("libpng", "assimp", "libjpeg-turbo", "nlohmann_json", "fx-gltf")

target("asset_manager")
    set_kind("static")
    add_files("src/asset_manager.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "parser", "asset", "utils")

