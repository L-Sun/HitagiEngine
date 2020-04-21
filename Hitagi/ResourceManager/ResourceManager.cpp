#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "PNG.hpp"
#include "JPEG.hpp"
#include "BMP.hpp"
#include "TGA.hpp"

#include "Assimp.hpp"

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
    m_SceneCache.clear();
    m_ImageCache.clear();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

std::shared_ptr<Image> ResourceManager::ParseImage(std::filesystem::path filePath) {
    std::string name = filePath.string();
    if (m_ImageCache.count(name) != 0) return m_ImageCache[name];

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
        return std::make_shared<Image>();
    }

    Image img = m_ImageParser[static_cast<unsigned>(format)]->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));
    if (img.GetDataSize() == 0) {
        m_Logger->error("Parse image failed.");
        return std::make_shared<Image>();
    }

    m_ImageCache.emplace(name, std::make_shared<Image>(std::move(img)));
    return m_ImageCache.at(name);
}

std::shared_ptr<Scene> ResourceManager::ParseScene(std::filesystem::path filePath) {
    std::string name = filePath.string();
    if (m_SceneCache.count(name) != 0) return m_SceneCache[name];

    auto scene = m_SceneParser->Parse(g_FileIOManager->SyncOpenAndReadBinary(filePath));

    if (scene) m_SceneCache[name] = scene;

    return scene;
}

}  // namespace Hitagi::Resource
