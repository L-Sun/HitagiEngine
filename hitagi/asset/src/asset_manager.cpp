#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/core/core.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/asset/parser/png.hpp>
#include <hitagi/asset/parser/jpeg.hpp>
#include <hitagi/asset/parser/bmp.hpp>
#include <hitagi/asset/parser/tga.hpp>
#include <hitagi/asset/parser/assimp.hpp>

// #include <hitagi/asset/parser/bvh.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi::asset {

AssetManager::AssetManager(std::filesystem::path asset_base_path)
    : RuntimeModule("AssetManager"),
      m_BasePath(std::move(asset_base_path)) {
    if (core::FileIOManager::Get() == nullptr) {
        m_Logger->warn("File IO Manager is not initialized!");
    }

    m_MaterialParser = std::make_shared<MaterialJSONParser>();

    m_ImageParsers[ImageFormat::PNG]  = std::make_shared<PngParser>(m_Logger);
    m_ImageParsers[ImageFormat::JPEG] = std::make_shared<JpegParser>(m_Logger);
    m_ImageParsers[ImageFormat::TGA]  = std::make_shared<TgaParser>(m_Logger);
    m_ImageParsers[ImageFormat::BMP]  = std::make_shared<BmpParser>(m_Logger);

    m_SceneParsers[SceneFormat::UNKOWN] = std::make_shared<AssimpParser>(
        m_ImageParsers,
        [this](auto name) { return GetMaterial(name); },
        m_Logger);
    m_SceneParsers[SceneFormat::GLTF]  = m_SceneParsers[SceneFormat::UNKOWN];
    m_SceneParsers[SceneFormat::GLB]   = m_SceneParsers[SceneFormat::UNKOWN];
    m_SceneParsers[SceneFormat::BLEND] = m_SceneParsers[SceneFormat::UNKOWN];
    m_SceneParsers[SceneFormat::FBX]   = m_SceneParsers[SceneFormat::UNKOWN];

    // m_MoCapParser = std::make_unique<BvhParser>();

    InitBuiltinMaterial();
}

AssetManager::~AssetManager() {
    // m_MoCapParser = nullptr;
    Texture::DestroyDefaultTexture();
}

std::shared_ptr<Scene> AssetManager::ImportScene(const std::filesystem::path& path) {
    auto format = get_scene_format(path.extension().string());
    auto scene  = m_SceneParsers[format]->Parse(path, path.parent_path());
    AddScene(scene);
    return scene;
}

std::shared_ptr<Texture> AssetManager::ImportTexture(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());
    auto image  = m_ImageParsers[format]->Parse(path);
    AddTexture(image);
    return image;
}

std::shared_ptr<Material> AssetManager::ImportMaterial(const std::filesystem::path& path) {
    auto&& [iter, success] = m_Assets.materials.emplace(m_MaterialParser->Parse(path));
    return *iter;
}

void AssetManager::AddScene(std::shared_ptr<Scene> scene) {
    if (scene == nullptr) return;
    m_Assets.scenes.emplace(scene);
}

void AssetManager::AddCamera(std::shared_ptr<Camera> camera) {
    if (camera) m_Assets.cameras.emplace(std::move(camera));
}

void AssetManager::AddLight(std::shared_ptr<Light> light) {
    if (light) m_Assets.lights.emplace(std::move(light));
}

void AssetManager::AddMesh(std::shared_ptr<Mesh> mesh) {
    if (mesh) m_Assets.meshes.emplace(std::move(mesh));
}

void AssetManager::AddSkeleton(std::shared_ptr<Skeleton> skeleton) {
    if (skeleton) m_Assets.skeletons.emplace(std::move(skeleton));
}

void AssetManager::AddTexture(std::shared_ptr<Texture> texture) {
    if (texture) m_Assets.textures.emplace(std::move(texture));
}

auto AssetManager::GetMaterial(std::string_view name) -> std::shared_ptr<Material> {
    auto iter = std::find_if(m_Assets.materials.begin(), m_Assets.materials.end(), [&](const std::shared_ptr<Material>& mat) {
        return mat->GetName() == name;
    });

    return iter != m_Assets.materials.end() ? *iter : nullptr;
}

void AssetManager::InitBuiltinMaterial() {
    auto material_path = m_BasePath / "materials";
    if (!std::filesystem::exists(m_BasePath / "materials")) {
        m_Logger->warn("Missing material folder: assets/materials");
        return;
    }
    for (const auto& material_file : std::filesystem::directory_iterator(material_path)) {
        if (material_file.is_regular_file() && material_file.path().extension() == ".json") {
            m_Logger->info("Load built in material: {}", material_file.path().string());
            m_Assets.materials.emplace(m_MaterialParser->Parse(material_file.path()));
        }
    }
}

}  // namespace hitagi::asset
