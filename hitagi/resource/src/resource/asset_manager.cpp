#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/core/core.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/math/vector.hpp>
#include <hitagi/parser/png.hpp>
#include <hitagi/parser/jpeg.hpp>
#include <hitagi/parser/bmp.hpp>
#include <hitagi/parser/tga.hpp>
#include <hitagi/parser/assimp.hpp>

// #include <hitagi/parser/bvh.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi {
resource::AssetManager* asset_manager = nullptr;
}

namespace hitagi::resource {

bool AssetManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("AssetManager");
    m_Logger->info("Initialize...");

    m_MaterialParser = std::make_unique<MaterialJSONParser>();

    m_SceneParsers[SceneFormat::UNKOWN] = std::make_unique<AssimpParser>("");
    m_SceneParsers[SceneFormat::GLTF]   = std::make_unique<AssimpParser>(".gltf");
    m_SceneParsers[SceneFormat::GLB]    = std::make_unique<AssimpParser>(".glb");
    m_SceneParsers[SceneFormat::BLEND]  = std::make_unique<AssimpParser>(".blend");
    m_SceneParsers[SceneFormat::FBX]    = std::make_unique<AssimpParser>(".fbx");

    m_ImageParsers[ImageFormat::PNG]  = std::make_unique<PngParser>();
    m_ImageParsers[ImageFormat::JPEG] = std::make_unique<JpegParser>();
    m_ImageParsers[ImageFormat::TGA]  = std::make_unique<TgaParser>();
    m_ImageParsers[ImageFormat::BMP]  = std::make_unique<BmpParser>();

    // m_MoCapParser = std::make_unique<BvhParser>();

    InitBuiltinMaterial();

    return true;
}

void AssetManager::Tick() {}

void AssetManager::Finalize() {
    // m_MoCapParser = nullptr;

    m_MaterialParser = nullptr;
    m_ImageParsers.clear();
    m_SceneParsers.clear();
    m_Assets = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::shared_ptr<Scene> AssetManager::ImportScene(const std::filesystem::path& path) {
    auto format = get_scene_format(path.extension().string());
    auto scene  = m_SceneParsers[format]->Parse(file_io_manager->SyncOpenAndReadBinary(path), path.parent_path());
    AddScene(scene);
    return scene;
}

std::shared_ptr<Texture> AssetManager::ImportTexture(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());
    return ImportTexture(file_io_manager->SyncOpenAndReadBinary(path), format);
}

std::shared_ptr<Texture> AssetManager::ImportTexture(const core::Buffer& buffer, ImageFormat format) {
    if (format == ImageFormat::UNKOWN) {
        m_Logger->error("Unkown image format, and return a null");
        return nullptr;
    }

    auto image = m_ImageParsers[format]->Parse(buffer);
    AddTexture(image);
    return image;
}

std::shared_ptr<Material> AssetManager::ImportMaterial(const std::filesystem::path& path) {
    return AddMaterial(m_MaterialParser->Parse(file_io_manager->SyncOpenAndReadBinary(path)));
}

std::shared_ptr<Material> AssetManager::AddMaterial(std::shared_ptr<Material> material) {
    if (material == nullptr) return nullptr;

    if (m_Assets.materials.contains(material)) return material;

    auto iter = std::find_if(m_Assets.materials.begin(), m_Assets.materials.end(), [material](const std::shared_ptr<Material>& mat) {
        return *material == *mat;
    });

    if (iter != m_Assets.materials.end()) return *iter;

    m_Assets.materials.emplace(material);
    return material;
}

void AssetManager::AddScene(std::shared_ptr<Scene> scene) {
    if (scene) m_Assets.scenes.emplace(std::move(scene));
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

void AssetManager::AddArmature(std::shared_ptr<Armature> armature) {
    if (armature) m_Assets.armatures.emplace(std::move(armature));
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
    auto material_path = config_manager->GetConfig().asset_root_path / "materials";
    if (!std::filesystem::exists(material_path)) {
        m_Logger->warn("Missing material folder: assets/materials");
        return;
    }
    for (const auto& material_file : std::filesystem::directory_iterator(material_path)) {
        if (material_file.is_regular_file() && material_file.path().extension() == ".json") {
            m_Logger->debug("Load built in material: {}", material_file.path().string());
            AddMaterial(m_MaterialParser->Parse(file_io_manager->SyncOpenAndReadBinary(material_file.path())));
        }
    }
}

}  // namespace hitagi::resource
