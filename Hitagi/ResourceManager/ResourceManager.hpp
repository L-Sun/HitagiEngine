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

    Image ParseImage(const std::filesystem::path& filePath);
    Scene ParseScene(const std::filesystem::path& filePath);

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<unsigned>(ImageFormat::NUM_SUPPORT)> m_ImageParser;
    std::unique_ptr<SceneParser>                                                              m_SceneParser;
};
}  // namespace Hitagi::Resource

namespace Hitagi {
extern std::unique_ptr<Resource::ResourceManager> g_ResourceManager;
}
