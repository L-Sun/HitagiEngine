#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "PNG.hpp"
#include "JPEG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

#include "Assimp.hpp"

namespace Hitagi {
std::unique_ptr<Resource::ResourceManager> g_ResourceManager = std::make_unique<Resource::ResourceManager>();
}

namespace Hitagi::Resource {

int ResourceManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("ResourceManager");
    m_Logger->info("Initialize...");

    m_ImageParser[static_cast<unsigned>(ImageFormat::PNG)]  = std::make_unique<PngParser>();
    m_ImageParser[static_cast<unsigned>(ImageFormat::JPEG)] = std::make_unique<JpegParser>();
    m_ImageParser[static_cast<unsigned>(ImageFormat::TGA)]  = std::make_unique<TgaParser>();
    m_ImageParser[static_cast<unsigned>(ImageFormat::BMP)]  = std::make_unique<BmpParser>();

    m_SceneParser = std::make_unique<AssimpParser>();
    return 0;
}

void ResourceManager::Tick() {}
void ResourceManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

Image ResourceManager::ParseImage(const std::filesystem::path& filePath) {
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
    Image img = m_ImageParser[static_cast<unsigned>(format)]->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));
    if (img.GetDataSize() == 0) {
        m_Logger->error("Parse image failed.");
        return Image{};
    }

    return img;
}

Scene ResourceManager::ParseScene(const std::filesystem::path& filePath) {
    return m_SceneParser->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));
}

}  // namespace Hitagi::Resource
