#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/parser/image_parser.hpp>
// #include <hitagi/parser/mocap_parser.hpp>
// #include <hitagi/parser/scene_parser.hpp>

#include <map>

namespace hitagi::asset {

class AssetManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    // std::shared_ptr<Scene> ImportScene(const std::filesystem::path& path);
    std::shared_ptr<Image> ImportImage(const std::filesystem::path& path);

    // std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> ImportAnimation(const std::filesystem::path& path);

private:
    std::array<std::unique_ptr<ImageParser>, static_cast<size_t>(ImageFormat::NUM_SUPPORT)> m_ImageParser;

    // std::unique_ptr<SceneParser> m_SceneParser;

    // std::unique_ptr<MoCapParser> m_MoCapParser;
};
}  // namespace hitagi::asset

namespace hitagi {
extern std::unique_ptr<asset::AssetManager> g_AssetManager;
}
