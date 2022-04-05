#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/parser/png.hpp>
#include <hitagi/parser/jpeg.hpp>
#include <hitagi/parser/bmp.hpp>
#include <hitagi/parser/tga.hpp>
// #include <hitagi/parser/assimp.hpp>
// #include <hitagi/parser/bvh.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi {
std::unique_ptr<asset::AssetManager> g_AssetManager = std::make_unique<asset::AssetManager>();
}

namespace hitagi::asset {

int AssetManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("AssetManager");
    m_Logger->info("Initialize...");

    // m_SceneParser = std::make_unique<AssimpParser>();

    m_ImageParser[static_cast<size_t>(ImageFormat::PNG)]  = std::make_unique<PngParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::JPEG)] = std::make_unique<JpegParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::TGA)]  = std::make_unique<TgaParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::BMP)]  = std::make_unique<BmpParser>();

    // m_MoCapParser = std::make_unique<BvhParser>();

    return 0;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    // m_MoCapParser = nullptr;

    for (auto&& img_parser : m_ImageParser) {
        img_parser = nullptr;
    }

    // m_SceneParser = nullptr;

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

// std::shared_ptr<Scene> AssetManager::ImportScene(const std::filesystem::path& path) {
//     auto scene = m_SceneParser->Parse(g_FileIoManager->SyncOpenAndReadBinary(path));
//     scene->SetName(path.stem().string());
//     if (!scene) m_Logger->error("Failed to Import Scene: {}", path.string());

//     return scene;
// }

std::shared_ptr<Image> AssetManager::ImportImage(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());

    if (format >= ImageFormat::NUM_SUPPORT) {
        m_Logger->error("Unkown image format, and return a null");
        return nullptr;
    }
    auto image = m_ImageParser[static_cast<size_t>(format)]->Parse(g_FileIoManager->SyncOpenAndReadBinary(path));
    return image;
}

// std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> AssetManager::ImportAnimation(const std::filesystem::path& path) {
//     auto format = get_mocap_format(path.extension().string());

//     if (format >= MoCapFormat::NUM_SUPPORT) {
//         m_Logger->error("Unkown MoCap format, and return a null");
//         return {nullptr, nullptr};
//     }

//     auto result = m_MoCapParser->Parse(g_FileIoManager->SyncOpenAndReadBinary(path));
//     result.second->SetName(path.stem().string());
//     return result;
// }

}  // namespace hitagi::asset
