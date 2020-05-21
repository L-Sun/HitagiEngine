#pragma once
#include <map>

#include "FileIOManager.hpp"
#include "ImageParser.hpp"
#include "SceneParser.hpp"

namespace Hitagi::Resource {

class ResourceManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::shared_ptr<Image> ParseImage(const std::filesystem::path& filePath);
    std::shared_ptr<Scene> ParseScene(const std::filesystem::path& filePath);

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<unsigned>(ImageFormat::NUM_SUPPORT)> m_ImageParser;
    std::unique_ptr<SceneParser>                                                              m_SceneParser;

    std::map<std::string, std::pair<std::filesystem::file_time_type, std::shared_ptr<Image>>> m_ImageCache;
    std::map<std::string, std::pair<std::filesystem::file_time_type, std::shared_ptr<Scene>>> m_SceneCache;
};
}  // namespace Hitagi::Resource

namespace Hitagi {
extern std::unique_ptr<Resource::ResourceManager> g_ResourceManager;
}
