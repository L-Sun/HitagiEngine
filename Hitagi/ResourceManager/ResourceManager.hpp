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

    std::shared_ptr<Image> ParseImage(std::filesystem::path filePath);
    std::shared_ptr<Scene> ParseScene(std::filesystem::path filePath);

private:
    std::unique_ptr<ImageParser> m_ImageParser[static_cast<unsigned>(ImageFormat::NUM_SUPPORT)];
    std::unique_ptr<SceneParser> m_SceneParser;

    std::map<std::string, std::shared_ptr<Image>> m_ImageCache;
    std::map<std::string, std::shared_ptr<Scene>> m_SceneCache;
};
}  // namespace Hitagi::Resource

namespace Hitagi {
extern std::unique_ptr<Resource::ResourceManager> g_ResourceManager;
}
