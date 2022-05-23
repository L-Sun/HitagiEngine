#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/parser/image_parser.hpp>
// #include <hitagi/parser/mocap_parser.hpp>
#include <hitagi/parser/scene_parser.hpp>

#include <magic_enum.hpp>

namespace hitagi::resource {

class AssetManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    Scene CreateEmptyScene(std::string_view name);

    void                   ImportScene(Scene& scene, const std::filesystem::path& path);
    std::shared_ptr<Image> ImportImage(const std::filesystem::path& path);

    // std::pair<std::shared_ptr<BoneNode>, std::shared_ptr<Animation>> ImportAnimation(const std::filesystem::path& path);

private:
    void InitializeInnerMaterial();

    // Parser
    std::array<std::unique_ptr<ImageParser>, magic_enum::enum_count<ImageFormat>()> m_ImageParser;
    std::unique_ptr<SceneParser>                                                    m_SceneParser;

    std::pmr::vector<std::shared_ptr<Material>> m_Materials;
};
}  // namespace hitagi::resource

namespace hitagi {
extern std::unique_ptr<resource::AssetManager> g_AssetManager;
}
