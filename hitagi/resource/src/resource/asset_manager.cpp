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

    m_SceneParser = std::make_unique<AssimpParser>();

    m_ImageParser[static_cast<size_t>(ImageFormat::PNG)]  = std::make_unique<PngParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::JPEG)] = std::make_unique<JpegParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::TGA)]  = std::make_unique<TgaParser>();
    m_ImageParser[static_cast<size_t>(ImageFormat::BMP)]  = std::make_unique<BmpParser>();

    m_MaterialJSONParser = std::make_unique<MaterialJSONParser>();

    // m_MoCapParser = std::make_unique<BvhParser>();

    return true;
}

void AssetManager::Tick() {}
void AssetManager::Finalize() {
    // m_MoCapParser = nullptr;

    m_MaterialJSONParser = nullptr;

    for (auto&& img_parser : m_ImageParser) {
        img_parser = nullptr;
    }

    m_SceneParser = nullptr;

    for (auto&& materail : m_Materials) {
        materail = nullptr;
    }

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void AssetManager::ImportScene(Scene& scene, const std::filesystem::path& path) {
    m_SceneParser->Parse(file_io_manager->SyncOpenAndReadBinary(path), scene, m_Materials);
}

std::shared_ptr<Image> AssetManager::ImportImage(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());

    if (format == ImageFormat::UNKOWN) {
        m_Logger->error("Unkown image format, and return a null");
        return nullptr;
    }
    auto image = m_ImageParser[static_cast<size_t>(format)]->Parse(file_io_manager->SyncOpenAndReadBinary(path));
    return image;
}

std::shared_ptr<MaterialInstance> AssetManager::ImportMaterial(const std::filesystem::path& path) {
    auto material = m_MaterialJSONParser->Parse(file_io_manager->SyncOpenAndReadBinary(path));
    if (material == nullptr) return nullptr;

    auto instance = material->CreateInstance();

    auto iter = std::find_if(m_Materials.begin(), m_Materials.end(), [&material](const std::shared_ptr<Material>& mat) {
        return *mat == *material;
    });
    if (iter != m_Materials.end()) {
        instance->SetMaterial(*iter);
    } else {
        m_Materials.emplace_back(std::move(material));
    }

    return instance;
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
