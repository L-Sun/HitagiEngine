#include "test_macros.hpp"

import asset;
import core;
import gfx;
import math;
import utils;
import test_utils;

using namespace hitagi;
using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::testing;

namespace {

constexpr std::string_view kTestImagePath = "assets/test/test.png";

auto ReadStoredHandle(const Material& material) -> gfx::BindlessHandle {
    const auto passes = material.GetPasses();
    if (passes.empty() || passes[0].material_data.GetDataSize() < sizeof(gfx::BindlessHandle)) return {};
    gfx::BindlessHandle handle;
    std::memcpy(&handle, passes[0].material_data.GetData(), sizeof(handle));
    return handle;
}

// Services every AssetManager under test depends on. Declared on the fixture so
// they outlive the manager (and any texture/scene that captured them).
class AssetFixture : public ::testing::Test {
protected:
    core::FileIOManager file_io;
    core::JobSystem     job_system;
};

using AssetRegistryTest    = AssetFixture;
using SceneResidencyTest   = AssetFixture;
using AsyncTextureLoadTest = AssetFixture;

}  // namespace

// ---------------------------------------------------------------------------
// P0: identity & deduplication
// ---------------------------------------------------------------------------

TEST_F(AssetRegistryTest, AcquireTextureDeduplicatesByPath) {
    AssetManager assets(file_io, job_system, "assets");

    const auto first  = assets.AcquireTexture(kTestImagePath);
    const auto second = assets.AcquireTexture(kTestImagePath);
    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);

    const auto other = assets.AcquireTexture("assets/test/test.jpg");
    ASSERT_TRUE(other);
    EXPECT_NE(first, other);
}

TEST_F(AssetRegistryTest, AcquireTextureNormalizesPath) {
    AssetManager assets(file_io, job_system, "assets");

    const auto direct  = assets.AcquireTexture(kTestImagePath);
    const auto indirect = assets.AcquireTexture("assets/test/../test/test.png");
    EXPECT_EQ(direct, indirect);
}

TEST_F(AssetRegistryTest, RegistryDoesNotExtendTextureLifetime) {
    AssetManager assets(file_io, job_system, "assets");

    std::weak_ptr<Texture> observer;
    {
        const auto texture = assets.AcquireTexture(kTestImagePath);
        observer           = texture;
    }
    EXPECT_TRUE(observer.expired());

    // A fresh acquire after expiry yields a new live instance.
    const auto reacquired = assets.AcquireTexture(kTestImagePath);
    ASSERT_TRUE(reacquired);
    EXPECT_TRUE(observer.expired());
}

TEST_F(AssetRegistryTest, GetMaterialReflectsLifetime) {
    AssetManager assets(file_io, job_system, "assets");

    auto material = std::make_shared<Material>(MaterialParameters{}, std::pmr::vector<MaterialPass>{}, "TestRegistryMaterial");
    assets.AddMaterial(material);
    EXPECT_EQ(assets.GetMaterial("TestRegistryMaterial"), material);

    material.reset();
    EXPECT_EQ(assets.GetMaterial("TestRegistryMaterial"), nullptr);
}

TEST_F(AssetRegistryTest, FindResourceByUUID) {
    AssetManager assets(file_io, job_system, "assets");

    auto       texture = assets.AcquireTexture(kTestImagePath);
    const auto uuid    = texture->GetUUID();
    EXPECT_EQ(assets.FindResource(uuid), texture);

    texture.reset();
    EXPECT_EQ(assets.FindResource(uuid), nullptr);
}

TEST_F(AssetRegistryTest, ImportTextureDeduplicatesByPath) {
    AssetManager assets(file_io, job_system, "assets");

    const auto first  = assets.ImportTexture(kTestImagePath);
    const auto second = assets.ImportTexture(kTestImagePath);
    ASSERT_TRUE(first);
    EXPECT_EQ(first, second);
    // ImportTexture also unifies with lazy acquires of the same path.
    EXPECT_EQ(assets.AcquireTexture(kTestImagePath), first);
}

// ---------------------------------------------------------------------------
// P1: scene-level residency
// ---------------------------------------------------------------------------

TEST_F(SceneResidencyTest, UnloadSceneReleasesMeshAndMaterialGPUData) {
    // Device must outlive the AssetManager: its destructor releases the default
    // texture whose GPU view still references the device.
    auto         device = gfx::create_device(gfx::Device::Type::Mock, "SceneResidencyTest");
    AssetManager assets(file_io, job_system, "assets");

    const auto material = std::make_shared<Material>(
        MaterialParameters{{.name = "base_color", .value = vec4f{1, 1, 1, 1}}},
        std::pmr::vector<MaterialPass>{MaterialPass{.pass_contract = "Test", .bindings = {"base_color"}}},
        "SceneResidencyMaterial");

    const auto mesh                = MeshFactory::Cube();
    mesh->sub_meshes[0].material   = material;

    const auto scene = std::make_shared<Scene>("SceneResidencyScene");
    scene->CreateMeshEntity(mesh, mat4f::identity(), scene->GetRootEntity(), "cube");
    assets.AddScene(scene);

    // Simulate the renderer loading resources directly (bypassing Scene::Load).
    mesh->Load({.device = *device});
    material->Load({.device = *device});

    ASSERT_TRUE(mesh->vertices->GetAttributeData(VertexAttribute::Position).has_value());
    EXPECT_NE(mesh->vertices->GetAttributeData(VertexAttribute::Position)->get().gpu_buffer, nullptr);
    EXPECT_NE(mesh->indices->GetGPUData(), nullptr);
    EXPECT_FALSE(material->GetPasses()[0].material_data.Empty());

    assets.UnloadScene(scene);

    EXPECT_EQ(mesh->vertices->GetAttributeData(VertexAttribute::Position)->get().gpu_buffer, nullptr);
    EXPECT_EQ(mesh->indices->GetGPUData(), nullptr);
    EXPECT_TRUE(material->GetPasses()[0].material_data.Empty());
    EXPECT_EQ(assets.FindResource(scene->GetUUID()), nullptr);

    // CPU data must survive so the scene can come back cheaply.
    EXPECT_FALSE(mesh->vertices->Empty());
    mesh->Load({.device = *device});
    EXPECT_NE(mesh->indices->GetGPUData(), nullptr);
}

// ---------------------------------------------------------------------------
// P2: asynchronous texture loading
// ---------------------------------------------------------------------------

TEST_F(AsyncTextureLoadTest, FileBackedTextureLoadsAsynchronously) {
    auto         device = gfx::create_device(gfx::Device::Type::Mock, "AsyncTextureLoadTest");
    AssetManager assets(file_io, job_system, "assets");

    const ResourceLoadContext context{.device = *device};

    const auto texture = assets.AcquireTexture(kTestImagePath);
    ASSERT_TRUE(texture);
    EXPECT_EQ(texture->GetLoadState(), ResourceLoadState::Unloaded);

    // First Load only kicks off the decode job; no GPU data yet.
    texture->Load(context);
    EXPECT_FALSE(texture->GetLoadState() == ResourceLoadState::Loaded);
    EXPECT_EQ(texture->GetGPUView(), nullptr);

    job_system.WaitForAll();
    EXPECT_EQ(texture->GetLoadState(), ResourceLoadState::Staged);

    // Second Load uploads the decoded data on the caller ("render") thread.
    texture->Load(context);
    EXPECT_TRUE(texture->GetLoadState() == ResourceLoadState::Loaded);
    ASSERT_NE(texture->GetGPUView(), nullptr);
    EXPECT_EQ(texture->Width(), 278);
    EXPECT_EQ(texture->Height(), 152);
}

TEST_F(AsyncTextureLoadTest, MaterialUsesPlaceholderUntilTextureReady) {
    // Device must outlive the AssetManager (default-texture GPU view teardown).
    auto         device = gfx::create_device(gfx::Device::Type::Mock, "AsyncMaterialTest");
    AssetManager assets(file_io, job_system, "assets");

    const ResourceLoadContext context{.device = *device};

    const auto texture  = assets.AcquireTexture(kTestImagePath);
    const auto material = std::make_shared<Material>(
        MaterialParameters{{.name = "albedo", .value = texture}},
        std::pmr::vector<MaterialPass>{MaterialPass{.pass_contract = "Test", .bindings = {"albedo"}}},
        "AsyncMaterial");

    // Frame 1: decode in flight, the pass packs the placeholder handle.
    material->Load(context);
    EXPECT_FALSE(texture->GetLoadState() == ResourceLoadState::Loaded);
    const auto placeholder_view = Texture::DefaultTexture()->GetGPUView();
    ASSERT_NE(placeholder_view, nullptr);
    EXPECT_EQ(ReadStoredHandle(*material).index, placeholder_view->GetBindlessHandle().index);

    job_system.WaitForAll();

    // Frame 2: upload happens and the pass is repacked with the real handle.
    material->Load(context);
    ASSERT_TRUE(texture->GetLoadState() == ResourceLoadState::Loaded);
    ASSERT_NE(texture->GetGPUView(), nullptr);
    EXPECT_EQ(ReadStoredHandle(*material).index, texture->GetGPUView()->GetBindlessHandle().index);
    EXPECT_NE(texture->GetGPUView()->GetBindlessHandle().index, placeholder_view->GetBindlessHandle().index);

    // Frame 3: settled, Load is a no-op and the handle stays stable.
    material->Load(context);
    EXPECT_EQ(ReadStoredHandle(*material).index, texture->GetGPUView()->GetBindlessHandle().index);
}
