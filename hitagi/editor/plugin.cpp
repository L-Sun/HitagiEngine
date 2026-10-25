export module editor:plugin;
import app;
import core;
import ecs;
import asset;

import :scene_import;

export namespace hitagi {

struct UsdImportPlugin {
    asset::AssetService& assets;
    core::JobSystem&     jobs;
    UsdImportPolicy      usd{};

    void Build(App& app) const;
};

}  // namespace hitagi

namespace hitagi {

void UsdImportPlugin::Build(App& app) const {
    app.GetWorld().EmplaceResource<SceneImportService>(assets, jobs, usd);
}

}  // namespace hitagi
