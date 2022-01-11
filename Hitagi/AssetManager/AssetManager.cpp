#include "AssetManager.hpp"

#include "PNG.hpp"
#include "JPEG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

#include "BvhParser.hpp"

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

    m_MoCapParser = std::make_unique<BvhParser>();

    return 0;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    for (auto&& img_parser : m_ImageParser) {
        img_parser = nullptr;
    }

    m_MoCapParser = nullptr;

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::shared_ptr<Image> AssetManager::ImportImage(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());

    if (format >= ImageFormat::NUM_SUPPORT) {
        m_Logger->error("Unkown image format, and return a null");
        return nullptr;
    }
    auto image = m_ImageParser[static_cast<size_t>(format)]->Parse(g_FileIoManager->SyncOpenAndReadBinary(path));
    return image;
}

std::shared_ptr<BoneNode> AssetManager::ImportSkeleton(const std::filesystem::path& path) {
    auto format = get_mocap_format(path.extension().string());

    if (format >= MoCapFormat::NUM_SUPPORT) {
        m_Logger->error("Unkown MoCap format, and return a null");
        return nullptr;
    }

    return m_MoCapParser->ParserSkeleton(g_FileIoManager->SyncOpenAndReadBinary(path));
}

}  // namespace Hitagi::Asset
