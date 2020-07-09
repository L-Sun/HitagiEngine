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

    Image ParseImage(const std::filesystem::path& path) const;
    Scene ParseScene(const std::filesystem::path& path) const;

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<size_t>(ImageFormat::NUM_SUPPORT)> m_ImageParser;
    std::unique_ptr<SceneParser>                                                            m_SceneParser;
};
}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::AssetManager> g_AssetManager;
}
