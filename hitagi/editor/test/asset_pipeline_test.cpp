#include "interop/gtest_macros.hpp"
import interop.gtest;
import interop.spdlog;
import std;
import editor;
import engine;

using namespace hitagi;

namespace hitagi::asset_pipeline_tests {

auto ResolvePassTextures(const asset::Material& material, const asset::MaterialPass& pass) -> std::pmr::vector<std::shared_ptr<asset::Texture>> {
    std::pmr::vector<std::shared_ptr<asset::Texture>> textures;
    textures.reserve(pass.bindings.size());
    for (const auto& binding : pass.bindings) {
        const auto parameter = material.GetParameter<std::shared_ptr<asset::Texture>>(binding.name);
        if (parameter) textures.emplace_back(*parameter);
    }
    return textures;
}

// Core services for the tests that build an AssetManager; declared on the
// fixture so they outlive the manager and every texture it hands out.
class AssetPipelineTest : public ::testing::Test {
protected:
    core::FileIOManager file_io;
    core::JobSystem     job_system;
};

}  // namespace hitagi::asset_pipeline_tests

using namespace hitagi::asset_pipeline_tests;
int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST_F(AssetPipelineTest, MaterialDebugDataExposesSourceAndPassBindings) {
    auto base_color_texture = std::make_shared<asset::Texture>(
        1,
        1,
        gfx::Format::R8G8B8A8_UNORM,
        core::Buffer{},
        "base-color-debug");
    base_color_texture->SetPath("assets/test/base_color.png");

    auto material = std::make_shared<asset::Material>(
        asset::MaterialParameters{
            {.name = "base_color", .value = math::Color::White()},
            {.name = "roughness", .value = 0.5f},
            {.name = "base_color_texture", .value = base_color_texture},
        },
        std::pmr::vector<asset::MaterialPass>{
            asset::MaterialPass{
                .pass_contract = "DebugMaterial",
                .bindings = {{"base_color", math::Color::White()}, {"roughness", 0.5f}, {"base_color_texture", std::shared_ptr<asset::Texture>{}}},
            },
        },
        "debug-material");
    EditorCookContext cook_context;
    cook_context.SetMaterialSourceInfo(
        material,
        MaterialSourceInfo{
            .type                 = MaterialSourceType::Imported,
            .source_asset         = "assets/test/material_debug.usda",
            .source_material_path = "/Looks/DebugMaterial",
            .source_shader_id     = "TestSurface",
            .source_texture_paths = {"assets/test/base_color.png"},
            .unsupported_inputs   = {"clearcoat"},
        });

    const auto* pass = material->FindPass("DebugMaterial");
    ASSERT_NE(pass, nullptr);

    const auto* source_info = cook_context.FindMaterialSourceInfo(*material);
    ASSERT_NE(source_info, nullptr);
    EXPECT_EQ(source_info->source_asset, std::filesystem::path("assets/test/material_debug.usda"));
    EXPECT_EQ(source_info->source_shader_id, "TestSurface");
    ASSERT_EQ(source_info->unsupported_inputs.size(), 1);
    EXPECT_EQ(source_info->unsupported_inputs.front(), "clearcoat");

    ASSERT_EQ(pass->bindings.size(), 3);
    EXPECT_EQ(pass->bindings[0].name, "base_color");
    EXPECT_EQ(pass->bindings[1].name, "roughness");
    EXPECT_EQ(pass->bindings[2].name, "base_color_texture");

    const auto associated_textures = ResolvePassTextures(*material, *pass);
    ASSERT_EQ(associated_textures.size(), 1);
    EXPECT_EQ(associated_textures.front(), base_color_texture);
}

TEST_F(AssetPipelineTest, ImportsUsdSceneWithoutEditorWindow) {
    asset::AssetManager asset_manager(file_io, job_system, "assets");
    auto                scene = ImportEditorScene(
        asset_manager,
        "assets/test/test.usda",
        "assets/test",
        [&asset_manager](std::string_view name) { return asset_manager.GetMaterial(name); });

    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->GetRootEntity());
    EXPECT_EQ(GetEditorSceneFormat(".usda"), EditorSceneFormat::USDA);
}
