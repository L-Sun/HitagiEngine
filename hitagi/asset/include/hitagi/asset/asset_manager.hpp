#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/asset/parser/image_parser.hpp>
#include <hitagi/asset/parser/scene_parser.hpp>
#include <hitagi/asset/parser/material_parser.hpp>

#include <magic_enum.hpp>

namespace hitagi::asset {

class AssetManager final : public RuntimeModule {
public:
    AssetManager(std::filesystem::path asset_base_path);
    ~AssetManager() final;

    static auto Get() -> AssetManager* { return static_cast<AssetManager*>(GetModule("AssetManager")); }

    Scene CreateEmptyScene(std::string_view name);

    std::shared_ptr<Scene>    ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Texture>  ImportTexture(const std::filesystem::path& path);
    std::shared_ptr<Material> ImportMaterial(const std::filesystem::path& path);

    void AddScene(std::shared_ptr<Scene> scene);
    void AddCamera(std::shared_ptr<Camera> camera);
    void AddLight(std::shared_ptr<Light> light);
    void AddMesh(std::shared_ptr<Mesh> mesh);
    void AddSkeleton(std::shared_ptr<Skeleton> skeleton);
    void AddTexture(std::shared_ptr<Texture> texture);
    // void AddAnimation(std::shared_ptr<Animation> animation);

    std::shared_ptr<Material> GetMaterial(std::string_view name);
    inline const auto&        GetAllMaterials() const noexcept { return m_Assets.materials; }

private:
    void InitBuiltinMaterial();

    std::filesystem::path m_BasePath;

    // Parser
    std::shared_ptr<MaterialParser>                                    m_MaterialParser;
    std::pmr::unordered_map<ImageFormat, std::shared_ptr<ImageParser>> m_ImageParsers;
    std::pmr::unordered_map<SceneFormat, std::shared_ptr<SceneParser>> m_SceneParsers;

    struct Assets {
        template <typename T>
        using SharedPtrSet = std::pmr::set<std::shared_ptr<T>>;

        SharedPtrSet<Scene>    scenes;
        SharedPtrSet<Material> materials;
        SharedPtrSet<Camera>   cameras;
        SharedPtrSet<Light>    lights;
        SharedPtrSet<Mesh>     meshes;
        SharedPtrSet<Skeleton> skeletons;
        SharedPtrSet<Texture>  textures;
    } m_Assets;
};
}  // namespace hitagi::asset
