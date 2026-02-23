module;

#include <spdlog/logger.h>

export module asset:scene_parser;
import std;
import :scene;
import :image_parser;
import :material;
import utils;

export namespace hitagi::asset {
class AssetManager;

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
    SceneParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> = 0;

    virtual ~SceneParser() = default;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class AssimpParser : public SceneParser {
public:
    AssimpParser(utils::EnumArray<std::shared_ptr<ImageParser>, ImageFormat>       image_parsers,
                 std::function<std::shared_ptr<asset::Material>(std::string_view)> material_getter = {},
                 std::shared_ptr<spdlog::logger>                                   logger          = nullptr)
        : SceneParser(std::move(logger)), m_ImageParsers(std::move(image_parsers)), m_MaterialGetter(std::move(material_getter)) {}

    auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;

private:
    utils::EnumArray<std::shared_ptr<ImageParser>, ImageFormat>       m_ImageParsers;
    std::function<std::shared_ptr<asset::Material>(std::string_view)> m_MaterialGetter;
};
}  // namespace hitagi::asset
