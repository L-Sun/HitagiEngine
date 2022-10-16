#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
enum struct SceneFormat : std::uint8_t {
    UNKOWN,
    GLTF,
    GLB,
    BLEND,
    FBX,
};

constexpr inline SceneFormat get_scene_format(std::string_view ext) {
    if (ext == ".gltf")
        return SceneFormat::GLTF;
    if (ext == "glb")
        return SceneFormat::GLB;
    else if (ext == ".blend")
        return SceneFormat::BLEND;
    else if (ext == ".fbx")
        return SceneFormat::FBX;
    return SceneFormat::UNKOWN;
}

class SceneParser {
public:
    virtual std::shared_ptr<Scene> Parse(const core::Buffer& buffer, const std::filesystem::path& root_path) = 0;

    virtual ~SceneParser() = default;
};
}  // namespace hitagi::resource
