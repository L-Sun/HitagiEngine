#include "interop/gtest_macros.hpp"
import std;
import app;
import core;
import ecs;
import asset;
import editor;

import interop.gtest;

using namespace hitagi;

namespace hitagi::editor_plugin_tests {

auto FirstMaterial(const asset::Scene& scene) -> std::shared_ptr<asset::Material> {
    for (auto entity : scene.GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>()) {
        for (const auto& submesh : entity.Get<asset::MeshComponent>().mesh->sub_meshes) {
            if (submesh.material) return submesh.material;
        }
    }
    return {};
}

}  // namespace hitagi::editor_plugin_tests

using namespace hitagi::editor_plugin_tests;

TEST(EditorPluginTest, RetainsPolicyAfterPluginDestructionAndSeparatesApps) {
    core::Runtime       runtime({.worker_count = 2, .log_level = "off"});
    asset::AssetService assets(runtime.IO(), runtime.Jobs());
    std::weak_ptr<int>  retained;
    {
        App first(runtime);
        App second(runtime);
        {
            auto state = std::make_shared<int>(0);
            retained   = state;
            first.AddPlugin(UsdImportPlugin{
                .assets = assets,
                .jobs   = runtime.Jobs(),
                .usd    = {.material_processor = [state](EditorMaterial& material) {
                    ++*state;
                    material.material = material.material->Clone("FirstPolicy");
                }},
            });
        }
        second.AddPlugin(UsdImportPlugin{.assets = assets, .jobs = runtime.Jobs()});
        ASSERT_FALSE(retained.expired());
        auto& importer = first.GetWorld().GetResource<SceneImportService>();
        auto& other    = second.GetWorld().GetResource<SceneImportService>();
        EXPECT_NE(&importer, &other);
        const auto first_scene  = importer.Import("assets/test/test.usda");
        const auto second_scene = other.Import("assets/test/test.usda");
        ASSERT_TRUE(first_scene);
        ASSERT_TRUE(second_scene);
        const auto first_material  = FirstMaterial(*first_scene);
        const auto second_material = FirstMaterial(*second_scene);
        ASSERT_TRUE(first_material);
        ASSERT_TRUE(second_material);
        EXPECT_EQ(first_material->GetName(), "FirstPolicy");
        EXPECT_NE(second_material->GetName(), "FirstPolicy");
        EXPECT_GT(*retained.lock(), 0);
    }
    EXPECT_TRUE(retained.expired());
}

TEST(EditorPluginTest, CookContextBelongsToEachImport) {
    core::Runtime       runtime({.worker_count = 2, .log_level = "off"});
    asset::AssetService assets(runtime.IO(), runtime.Jobs());
    App                 app(runtime);
    app.AddPlugin(UsdImportPlugin{.assets = assets, .jobs = runtime.Jobs()});
    auto&             importer = app.GetWorld().GetResource<SceneImportService>();
    EditorCookContext first_context;
    EditorCookContext second_context;
    const auto        first_scene  = importer.Import("assets/test/test.usda", {.cook_context = &first_context});
    const auto        second_scene = importer.Import("assets/test/test.usda", {.cook_context = &second_context});
    ASSERT_TRUE(first_scene);
    ASSERT_TRUE(second_scene);
    const auto first_material  = FirstMaterial(*first_scene);
    const auto second_material = FirstMaterial(*second_scene);
    ASSERT_TRUE(first_material);
    ASSERT_TRUE(second_material);
    ASSERT_NE(first_context.FindMaterialSourceInfo(*first_material), nullptr);
    ASSERT_NE(second_context.FindMaterialSourceInfo(*second_material), nullptr);
    EXPECT_EQ(first_context.FindMaterialSourceInfo(*second_material), nullptr);
    EXPECT_EQ(second_context.FindMaterialSourceInfo(*first_material), nullptr);
    {
        EditorCookContext temporary;
        ASSERT_TRUE(importer.Import("assets/test/test.usda", {.cook_context = &temporary}));
    }
    // The previous context has expired; importing without one must not reuse it.
    ASSERT_TRUE(importer.Import("assets/test/test.usda"));
    first_context.Clear();
    EXPECT_NE(second_context.FindMaterialSourceInfo(*second_material), nullptr);
}

TEST(EditorPluginTest, CookedImportUsesAssetsWithoutApplyingUsdPolicy) {
    core::Runtime       runtime({.worker_count = 2, .log_level = "off"});
    asset::AssetService assets(runtime.IO(), runtime.Jobs());
    App                 app(runtime);
    unsigned            processed = 0;
    app.AddPlugin(UsdImportPlugin{
        .assets = assets,
        .jobs   = runtime.Jobs(),
        .usd    = {.material_processor = [&processed](EditorMaterial&) { ++processed; }},
    });
    const std::filesystem::path path = "temp/plugin-roundtrip.hcscene";
    std::filesystem::create_directories(path.parent_path());
    asset::Scene source("PluginRoundtrip");
    runtime.IO().SaveBuffer(path, CookEditorScene(source));
    auto&      importer = app.GetWorld().GetResource<SceneImportService>();
    const auto scene    = importer.Import(path);
    ASSERT_TRUE(scene);
    EXPECT_EQ(scene->GetName(), "PluginRoundtrip");
    EXPECT_EQ(assets.FindResource(scene->GetUUID()), scene);
    EXPECT_EQ(processed, 0u);
    EXPECT_FALSE(importer.Import("unsupported.extension"));
    EXPECT_FALSE(importer.Import("temp/nonexistent-plugin-test.usda"));
    std::filesystem::remove(path);
}
