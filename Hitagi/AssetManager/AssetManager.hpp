#pragma once
#include "FileIOManager.hpp"
#include "ImageParser.hpp"

#include <map>

namespace Hitagi::Asset {

class AssetManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::shared_ptr<Image> ImportImage(const std::filesystem::path& path);

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<size_t>(ImageFormat::NUM_SUPPORT)> m_ImageParser;

    std::unordered_map<xg::Guid, std::shared_ptr<Image>> m_ImportedImages;
};
}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::AssetManager> g_AssetManager;
}
