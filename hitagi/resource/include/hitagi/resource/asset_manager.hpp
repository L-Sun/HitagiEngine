#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/parser/image_parser.hpp>
#include <hitagi/parser/material_parser.hpp>
// #include <hitagi/parser/mocap_parser.hpp>
#include <hitagi/parser/scene_parser.hpp>

#include <magic_enum.hpp>

namespace hitagi::resource {

class AssetManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    Scene CreateEmptyScene(std::string_view name);

    std::optional<Scene>              ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Texture>          ImportImage(const std::filesystem::path& path);
    std::shared_ptr<MaterialInstance> ImportMaterial(const std::filesystem::path& path);

    // std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> ImportAnimation(const std::filesystem::path& path);

private:
    // Parser
    std::pmr::unordered_map<ImageFormat, std::unique_ptr<ImageParser>> m_ImageParsers;
    std::pmr::unordered_map<SceneFormat, std::unique_ptr<SceneParser>> m_SceneParsers;
    std::unique_ptr<MaterialJSONParser>                                m_MaterialJSONParser;

    std::pmr::vector<std::shared_ptr<Material>> m_Materials;
};
}  // namespace hitagi::resource

namespace hitagi {
extern resource::AssetManager* asset_manager;
}
