#pragma once
#include "FileIOManager.hpp"
#include "ImageParser.hpp"
#include "MoCapParser.hpp"
#include "SceneNode.hpp"

#include <map>

namespace Hitagi::Asset {

class AssetManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::shared_ptr<Image>    ImportImage(const std::filesystem::path& path);
    std::shared_ptr<BoneNode> ImportSkeleton(const std::filesystem::path& path);

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<size_t>(ImageFormat::NUM_SUPPORT)> m_ImageParser;
    std::unique_ptr<MoCapParser>                                                            m_MoCapParser;
};
}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::AssetManager> g_AssetManager;
}
