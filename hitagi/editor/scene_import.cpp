export module editor:scene_import;
import std;
import core;
import asset;

import interop.spdlog;

import :usd;
import :cook;

export namespace hitagi {

// Host-owned material semantics, copied into the service at installation.
struct UsdImportPolicy {
    UsdSceneImporter::MaterialGetter    material_getter;
    UsdSceneImporter::MaterialProcessor material_processor;
    std::shared_ptr<spdlog::logger>     logger;
};

struct SceneImportOptions {
    std::filesystem::path resource_base_path;
    // Borrowed only for this synchronous import; never retained by the service.
    EditorCookContext* cook_context = nullptr;
};

class SceneImportService {
public:
    SceneImportService(asset::AssetService& assets, core::JobSystem& jobs, UsdImportPolicy usd = {});
    auto Import(const std::filesystem::path& path, const SceneImportOptions& options = {}) const -> std::shared_ptr<asset::Scene>;

private:
    asset::AssetService& m_Assets;
    core::JobSystem&     m_Jobs;
    UsdImportPolicy      m_Usd;
};

}  // namespace hitagi

namespace hitagi {

SceneImportService::SceneImportService(asset::AssetService& assets, core::JobSystem& jobs, UsdImportPolicy usd)
    : m_Assets(assets), m_Jobs(jobs), m_Usd(std::move(usd)) {}

auto SceneImportService::Import(const std::filesystem::path& path, const SceneImportOptions& options) const -> std::shared_ptr<asset::Scene> {
    return ImportEditorScene(m_Assets, m_Jobs, path, options.resource_base_path,
                             m_Usd.material_getter, m_Usd.logger, m_Usd.material_processor, options.cook_context);
}

}  // namespace hitagi
