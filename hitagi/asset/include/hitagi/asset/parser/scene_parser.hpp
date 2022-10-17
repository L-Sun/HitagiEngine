#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/asset/scene.hpp>

namespace hitagi::asset {
enum struct SceneFormat : std::uint8_t {
    UNKOWN,
    GLTF,
    GLB,
    BLEND,
    FBX,
};

inline constexpr SceneFormat get_scene_format(std::string_view ext) noexcept {
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
    SceneParser(std::shared_ptr<spdlog::logger> logger = nullptr);
    virtual auto Parse(const core::Buffer& buffer, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> = 0;

    virtual ~SceneParser() = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace hitagi::asset
