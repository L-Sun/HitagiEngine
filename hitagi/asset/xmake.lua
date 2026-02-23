target("asset")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps("core", "math", "gfx", "ecs", "utils")
    add_packages("libpng", "assimp", "libjpeg-turbo", "nlohmann_json", "fx-gltf", "range-v3")

includes("test")
