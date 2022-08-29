#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/core/core.hpp>
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

    return true;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    // m_MoCapParser = nullptr;

    m_ImageParsers.clear();
    m_SceneParsers.clear();
    m_Assets = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::optional<Scene> AssetManager::ImportScene(const std::filesystem::path& path) {
    auto format = get_scene_format(path.extension().string());
    return m_SceneParsers[format]->Parse(file_io_manager->SyncOpenAndReadBinary(path), path.parent_path());
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
    return image;
}

// std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> AssetManager::ImportAnimation(const std::filesystem::path& path) {
//     auto format = get_mocap_format(path.extension().string());

//     if (format >= MoCapFormat::NUM_SUPPORT) {
//         m_Logger->error("Unkown MoCap format, and return a null");
//         return {nullptr, nullptr};
//     }

//     auto result = m_MoCapParser->Parse(file_io_manager->SyncOpenAndReadBinary(path));
//     result.second->SetName(path.stem().string());
//     return result;
// }

}  // namespace hitagi::resource
