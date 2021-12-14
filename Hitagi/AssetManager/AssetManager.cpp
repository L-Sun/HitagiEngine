#include "AssetManager.hpp"

#include "PNG.hpp"
#include "JPEG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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

    return 0;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::shared_ptr<Image> AssetManager::ImportImage(const std::filesystem::path& path) {
    ImageFormat format = ImageFormat::NUM_SUPPORT;

    auto ext = path.extension();
    if (ext == ".jpeg" || ext == ".jpg")
        format = ImageFormat::JPEG;
    else if (ext == ".bmp")
        format = ImageFormat::BMP;
    else if (ext == ".tga")
        format = ImageFormat::TGA;
    else if (ext == ".png")
        format = ImageFormat::PNG;

    if (format >= ImageFormat::NUM_SUPPORT) {
        m_Logger->error("Unkown image format, and return a null");
        return nullptr;
    }
    auto image = m_ImageParser[static_cast<size_t>(format)]->Parse(g_FileIoManager->SyncOpenAndReadBinary(path));
    m_ImportedImages.emplace(xg::newGuid(), image);
    return image;
}

}  // namespace Hitagi::Asset
