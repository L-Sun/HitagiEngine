#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/parser/image_parser.hpp>
#include <hitagi/parser/scene_parser.hpp>

#include <magic_enum.hpp>
#include "hitagi/resource/mesh.hpp"

namespace hitagi::resource {

class AssetManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    Scene CreateEmptyScene(std::string_view name);

    std::optional<Scene>     ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Texture> ImportTexture(const std::filesystem::path& path);
    std::shared_ptr<Texture> ImportTexture(const core::Buffer& buffer, ImageFormat format);

    void AddMaterial(std::shared_ptr<Material> material);
    void AddCamera(std::shared_ptr<Camera> camera);
    void AddLight(std::shared_ptr<Light> light);
    void AddMesh(std::shared_ptr<Mesh> mesh);
    void AddArmature(std::shared_ptr<Armature> armature);
    // void AddAnimation(std::shared_ptr<Animation> animation);

private:
    // Parser
    std::pmr::unordered_map<ImageFormat, std::unique_ptr<ImageParser>> m_ImageParsers;
    std::pmr::unordered_map<SceneFormat, std::unique_ptr<SceneParser>> m_SceneParsers;

public:
    struct Assets {
        template <typename T>
        using SharedPtrVector = std::pmr::vector<std::shared_ptr<T>>;

        SharedPtrVector<Scene>    scenes;
        SharedPtrVector<Material> materials;
        SharedPtrVector<Camera>   cameras;
        SharedPtrVector<Light>    lights;
        SharedPtrVector<Mesh>     meshes;
        SharedPtrVector<Armature> armatures;
        SharedPtrVector<Texture>  textures;
    } m_Assets;
};
}  // namespace hitagi::resource

namespace hitagi {
extern resource::AssetManager* asset_manager;
}
