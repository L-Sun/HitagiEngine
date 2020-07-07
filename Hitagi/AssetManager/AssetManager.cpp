#include "AssetManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "PNG.hpp"
#include "JPEG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

#include "Assimp.hpp"

namespace Hitagi {
std::unique_ptr<Asset::AssetManager> g_AssetManager = std::make_unique<Asset::AssetManager>();
}

namespace Hitagi::Asset {

int AssetManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("AssetManager");
    m_Logger->info("Initialize...");

    m_ImageParser[static_cast<size_t>(ImageFormat::PNG)]  = std::make_unique<PngParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::JPEG)] = std::make_unique<JpegParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::TGA)]  = std::make_unique<TgaParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::BMP)]  = std::make_unique<BmpParser>();

    m_SceneParser = std::make_unique<AssimpParser>();
    return 0;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

Image AssetManager::ParseImage(const std::filesystem::path& filePath) const {
    ImageFormat format = ImageFormat::NUM_SUPPORT;

    auto ext = filePath.extension();
    if (ext == ".jpeg" || ext == ".jpg")
        format = ImageFormat::JPEG;
    else if (ext == ".bmp")
        format = ImageFormat::BMP;
    else if (ext == ".tga")
        format = ImageFormat::TGA;
    else if (ext == ".png")
        format = ImageFormat::PNG;

    if (format >= ImageFormat::NUM_SUPPORT) {
        m_Logger->error("Unkown image format, and return a empty image");
        return Image{};
    }
    return m_ImageParser[static_cast<size_t>(format)]->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));
}

Scene AssetManager::ParseScene(const std::filesystem::path& filePath) const {
    return m_SceneParser->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));
}

}  // namespace Hitagi::Asset
