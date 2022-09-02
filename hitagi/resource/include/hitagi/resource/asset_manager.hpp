#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/parser/image_parser.hpp>
#include <hitagi/parser/scene_parser.hpp>
#include <hitagi/parser/material_parser.hpp>

#include <magic_enum.hpp>
#include "hitagi/resource/mesh.hpp"

namespace hitagi::resource {

class AssetManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "AssetManager"; }

    Scene CreateEmptyScene(std::string_view name);

    std::shared_ptr<Scene>    ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Texture>  ImportTexture(const std::filesystem::path& path);
    std::shared_ptr<Texture>  ImportTexture(const core::Buffer& buffer, ImageFormat format);
    std::shared_ptr<Material> ImportMaterial(const std::filesystem::path& path);

    // If there is a same material, then ignore the input material and return the exist material
    std::shared_ptr<Material> AddMaterial(std::shared_ptr<Material> material);

    void AddScene(std::shared_ptr<Scene> scene);
    void AddCamera(std::shared_ptr<Camera> camera);
    void AddLight(std::shared_ptr<Light> light);
    void AddMesh(std::shared_ptr<Mesh> mesh);
    void AddArmature(std::shared_ptr<Armature> armature);
    void AddTexture(std::shared_ptr<Texture> texture);
    // void AddAnimation(std::shared_ptr<Animation> animation);

    std::shared_ptr<Material> GetMaterial(std::string_view name);
    inline const auto&        GetAllMaterials() const noexcept { return m_Assets.materials; }

private:
    void InitBuiltinMaterial();

    // Parser
    std::unique_ptr<MaterialParser>                                    m_MaterialParser;
    std::pmr::unordered_map<ImageFormat, std::unique_ptr<ImageParser>> m_ImageParsers;
    std::pmr::unordered_map<SceneFormat, std::unique_ptr<SceneParser>> m_SceneParsers;

    struct Assets {
        template <typename T>
        using SharedPtrSet = std::pmr::set<std::shared_ptr<T>>;

        SharedPtrSet<Scene>    scenes;
        SharedPtrSet<Material> materials;
        SharedPtrSet<Camera>   cameras;
        SharedPtrSet<Light>    lights;
        SharedPtrSet<Mesh>     meshes;
        SharedPtrSet<Armature> armatures;
        SharedPtrSet<Texture>  textures;
    } m_Assets;
};
}  // namespace hitagi::resource

namespace hitagi {
extern resource::AssetManager* asset_manager;
}
