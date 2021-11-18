#pragma once
#include <map>

#include "FileIOManager.hpp"
#include "ImageParser.hpp"
#include "SceneParser.hpp"

namespace Hitagi::Asset {

class AssetManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::shared_ptr<Image> ImportImage(const std::filesystem::path& path);
    std::shared_ptr<Scene> ImportScene(const std::filesystem::path& path);

    auto GetScene(xg::Guid id) { return m_ImportedScenes.at(id); }
    auto GetImage(xg::Guid id) { return m_ImportedImages.at(id); }

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<size_t>(ImageFormat::NUM_SUPPORT)> m_ImageParser;
    std::unique_ptr<SceneParser>                                                            m_SceneParser;

    std::unordered_map<xg::Guid, std::shared_ptr<Scene>> m_ImportedScenes;
    std::unordered_map<xg::Guid, std::shared_ptr<Image>> m_ImportedImages;
};
}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::AssetManager> g_AssetManager;
}
