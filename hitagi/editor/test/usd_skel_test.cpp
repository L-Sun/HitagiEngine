#include "interop/gtest_macros.hpp"
import interop.gtest;
import interop.usd;
import std;
import editor;
import engine;

using namespace hitagi;

namespace usd_skel_test {
auto MeshNamed(const asset::Scene& scene, std::string_view name, std::string_view parent) -> ecs::Entity {
    for (auto entity : scene.GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>()) {
        auto parent_entity = entity.Get<asset::RelationShip>().parent;
        if (entity.Get<asset::MetaInfo>().name == name && parent_entity &&
            parent_entity.Get<asset::MetaInfo>().name == parent) return entity;
    }
    return {};
}

auto Write(std::string_view name, std::string_view source) -> std::filesystem::path {
    const auto path = std::filesystem::path("temp") / name;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << source;
    return path;
}

auto Source() -> std::string {
    std::ifstream file("examples/usd-viewer/assets/skinned.usda");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void Replace(std::string& source, std::string_view from, std::string_view to) {
    const auto offset = source.find(from);
    if (offset == std::string::npos) throw std::runtime_error("USD test fixture fragment not found");
    source.replace(offset, from.size(), to);
}
}  // namespace usd_skel_test

TEST(UsdSkelImportTest, UnboundMaterialResolvedBeforeMeshPublication) {
    core::JobSystem  jobs;
    auto             fallback = std::make_shared<asset::Material>(asset::MaterialParameters{}, std::pmr::vector<asset::MaterialPass>{}, "Default");
    UsdSceneImporter importer([&](std::string_view name) {
        EXPECT_TRUE(name.empty());
        return fallback;
    });
    const auto       path  = usd_skel_test::Write("usd-default-material.usda", R"(#usda 1.0
def Mesh "Triangle" {
    point3f[] points = [(0,0,0), (1,0,0), (0,1,0)]
    int[] faceVertexCounts = [3]
    int[] faceVertexIndices = [0,1,2]
}
)");
    auto             scene = importer.Import(jobs, path);
    ASSERT_TRUE(scene);
    const auto entities = scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>();
    ASSERT_EQ(entities.size(), 1u);
    EXPECT_EQ(entities.front().Get<asset::MeshComponent>().mesh->sub_meshes.front().material, fallback);
}

TEST(UsdRigImportTest, InstanceStateAndCookBoundary) {
    core::JobSystem jobs;
    auto source = usd_skel_test::Source();
    usd_skel_test::Replace(source, "def Skeleton \"Skeleton\" {", R"(def Skeleton "Skeleton" {
        custom int[] hitagi:rig:bodies = [1,1,0,1,65535]
        custom float[] hitagi:rig:properties = [0.1,0,0,1,0.5,0.5,0,0]
        custom matrix4d[] hitagi:rig:bind = [( (1,0,0,0), (0,1,0,0), (0,0,1,0), (0,0,0,1) )]
        custom matrix4d[] hitagi:rig:offsets = [( (1,0,0,0), (0,1,0,0), (0,0,1,0), (0,0,0,1) )]
        custom int[] hitagi:rig:joints = []
        custom matrix4d[] hitagi:rig:frames = []
        custom float[] hitagi:rig:limits = []
    )");
    auto scene = UsdSceneImporter{}.Import(jobs, usd_skel_test::Write("runtime-physics/import.usda", source));
    ASSERT_TRUE(scene);
    std::vector<const physics::RigWorld*> worlds;
    for (auto entity : scene->GetWorld().GetEntityManager().GetEntities()) {
        if (!entity.Has<physics::CharacterPhysics>()) continue;
        const auto& rig = entity.Get<physics::CharacterPhysics>();
        ASSERT_EQ(rig.bodies.size(), 1u);
        EXPECT_EQ(rig.joints[1].Get<asset::MetaInfo>().name, "Tip");
        ASSERT_TRUE(rig.world);
        worlds.emplace_back(rig.world.get());
    }
    ASSERT_TRUE(worlds.size() >= 2u);
    EXPECT_NE(worlds[0], worlds[1]);
    EXPECT_NO_THROW(scene->GetWorld().Update(jobs, 1.0 / 60.0));
    EXPECT_THROW(CookEditorScene(*scene), std::runtime_error);
    usd_skel_test::Replace(source, "properties = [0.1,0,0,1,0.5,0.5,0,0]", "properties = []");
    EXPECT_THROW(UsdSceneImporter{}.Import(jobs, usd_skel_test::Write("runtime-physics/invalid.usda", source)), std::runtime_error);
}

TEST(UsdRigImportTest, RejectsRemovedSpringPrototypeMetadata) {
    core::JobSystem jobs;
    auto source = usd_skel_test::Source();
    usd_skel_test::Replace(source, "def Skeleton \"Skeleton\" {", "def Skeleton \"Skeleton\" {\n custom int[] hitagi:spring:joints = [1]");
    EXPECT_THROW(UsdSceneImporter{}.Import(jobs, usd_skel_test::Write("runtime-physics/legacy.usda", source)), std::runtime_error);
}

TEST(UsdSkelImportTest, NativeInstancesShareMeshAndMatchUsdReferenceDeformation) {
    core::JobSystem jobs;
    const auto path  = std::filesystem::path("examples/usd-viewer/assets/skinned.usda");
    const auto scene = UsdSceneImporter{}.Import(jobs, path);
    ASSERT_TRUE(scene);
    ASSERT_EQ(scene->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().size(), 4u);
    const auto left  = usd_skel_test::MeshNamed(*scene, "Body", "Left");
    const auto right = usd_skel_test::MeshNamed(*scene, "Body", "Right");
    const auto badge = usd_skel_test::MeshNamed(*scene, "Badge", "Left");
    ASSERT_TRUE(left && right && badge);
    EXPECT_EQ(left.Get<asset::MeshComponent>().mesh, right.Get<asset::MeshComponent>().mesh);
    EXPECT_EQ(left.Get<asset::SkinnedMesh>().skin, right.Get<asset::SkinnedMesh>().skin);
    EXPECT_NE(left.Get<asset::SkinnedMesh>().joints[0], right.Get<asset::SkinnedMesh>().joints[0]);
    EXPECT_EQ(left.Get<asset::SkinnedMesh>().joints[0], badge.Get<asset::SkinnedMesh>().joints[0]);
    const auto player = left.Get<asset::SkinnedMesh>().joints[0].Get<asset::AnimationTarget>().player;
    EXPECT_DOUBLE_EQ(player.Get<asset::AnimationPlayer>().clip->GetDuration(), 1.0);
    const auto indices = left.Get<asset::MeshComponent>().mesh->vertices->Span<asset::VertexAttribute::BlendIndex>();
    ASSERT_EQ(indices.size(), 6u);
    const std::array<unsigned, 6> expected{1, 1, 0, 1, 0, 0};
    for (unsigned i = 0; i < 6; ++i) EXPECT_EQ(indices[i][0], expected[i]);

    const auto        stage = pxr::UsdStage::Open(path.generic_string());
    pxr::UsdSkelCache cache;
    const auto        root = stage->GetPrimAtPath(pxr::SdfPath("/World/Left"));
    ASSERT_TRUE(cache.Populate(pxr::UsdSkelRoot(root), pxr::UsdTraverseInstanceProxies()));
    const auto mesh_prim  = stage->GetPrimAtPath(pxr::SdfPath("/World/Left/Body"));
    const auto skin_query = cache.GetSkinningQuery(mesh_prim);
    const auto skel_query = cache.GetSkelQuery(pxr::UsdSkelBindingAPI(mesh_prim).GetInheritedSkeleton());
    for (double seconds : {0.0, 0.25, 0.5, 0.75}) {
        auto mutable_player                               = player;
        mutable_player.Get<asset::AnimationPlayer>().time = seconds;
        scene->GetWorld().Update(jobs);
        pxr::VtArray<pxr::GfMatrix4d> palette;
        pxr::VtArray<pxr::GfVec3f>    points;
        ASSERT_TRUE(skel_query.ComputeSkinningTransforms(&palette, pxr::UsdTimeCode(24.0 + seconds * 24.0)));
        ASSERT_TRUE(pxr::UsdGeomMesh(mesh_prim).GetPointsAttr().Get(&points));
        ASSERT_TRUE(skin_query.ComputeSkinnedPoints(palette, &points, pxr::UsdTimeCode(24.0 + seconds * 24.0)));
        const auto                    output = left.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>();
        const std::array<unsigned, 6> corners{0, 1, 2, 0, 2, 3};
        for (unsigned i = 0; i < 6; ++i)
            for (unsigned axis = 0; axis < 3; ++axis)
                EXPECT_NEAR(output[i][axis], points[corners[i]][axis], 1e-5f);
    }
    scene->GetWorld().Update(jobs, 0.5);
    EXPECT_NE(left.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2],
              right.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2]);
}

TEST(UsdSkelImportTest, GeometryBindAndSkeletonPlacementMatchUsdWorldSpace) {
    core::JobSystem jobs;
    auto source = usd_skel_test::Source();
    usd_skel_test::Replace(source, "def Skeleton \"Skeleton\" {", R"(def Skeleton "Skeleton" {
        double3 xformOp:translate = (0,3,2)
        uniform token[] xformOpOrder = ["xformOp:translate"])");
    usd_skel_test::Replace(source, "uniform token subdivisionScheme = \"none\"", R"(
        double3 xformOp:translate = (4,5,6)
        uniform token[] xformOpOrder = ["xformOp:translate"]
        uniform token subdivisionScheme = "none")");
    usd_skel_test::Replace(source,
                           "matrix4d primvars:skel:geomBindTransform = ( (1,0,0,0), (0,1,0,0), (0,0,1,0), (0,0,0,1) )",
                           "matrix4d primvars:skel:geomBindTransform = ( (2,0,0,0), (0,1,0,0), (0,0,1,0), (0.4,0.5,0,1) )");
    const auto path  = usd_skel_test::Write("usd-skel-spaces.usda", source);
    const auto scene = UsdSceneImporter{}.Import(jobs, path);
    ASSERT_TRUE(scene);
    scene->GetWorld().Update(jobs, 0.25);
    const auto mesh = usd_skel_test::MeshNamed(*scene, "Body", "Left");
    ASSERT_TRUE(mesh);
    const auto        stage = pxr::UsdStage::Open(path.generic_string());
    pxr::UsdSkelCache cache;
    ASSERT_TRUE(cache.Populate(pxr::UsdSkelRoot(stage->GetPrimAtPath(pxr::SdfPath("/World/Left"))), pxr::UsdTraverseInstanceProxies()));
    const auto                    prim     = stage->GetPrimAtPath(pxr::SdfPath("/World/Left/Body"));
    const auto                    skeleton = pxr::UsdSkelBindingAPI(prim).GetInheritedSkeleton();
    const auto                    query    = cache.GetSkelQuery(skeleton);
    pxr::VtArray<pxr::GfMatrix4d> palette;
    pxr::VtArray<pxr::GfVec3f>    points;
    ASSERT_TRUE(query.ComputeSkinningTransforms(&palette, pxr::UsdTimeCode(30)));
    ASSERT_TRUE(pxr::UsdGeomMesh(prim).GetPointsAttr().Get(&points));
    ASSERT_TRUE(cache.GetSkinningQuery(prim).ComputeSkinnedPoints(palette, &points, pxr::UsdTimeCode(30)));
    const auto                    skel_world = pxr::UsdGeomXformable(skeleton.GetPrim()).ComputeLocalToWorldTransform(pxr::UsdTimeCode(30));
    const auto                    output     = mesh.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>();
    const std::array<unsigned, 6> corners{0, 1, 2, 0, 2, 3};
    for (unsigned i = 0; i < 6; ++i) {
        const auto actual   = mesh.Get<asset::Transform>().world_matrix * math::vec4f(output[i], 1.0f);
        const auto expected = skel_world.Transform(points[corners[i]]);
        for (unsigned axis = 0; axis < 3; ++axis) EXPECT_NEAR(actual[axis], expected[axis], 1e-5f);
    }
}

TEST(UsdSkelImportTest, RestPoseAndImplicitAnimationRange) {
    core::JobSystem jobs;
    auto source = usd_skel_test::Source();
    usd_skel_test::Replace(source, "    startTimeCode = 24\n", "");
    usd_skel_test::Replace(source, "    endTimeCode = 48\n", "");
    usd_skel_test::Replace(source, "rel skel:animationSource = </SwingRight>", "");
    const auto scene = UsdSceneImporter{}.Import(jobs, usd_skel_test::Write("usd-skel-rest.usda", source));
    ASSERT_TRUE(scene);
    auto left  = usd_skel_test::MeshNamed(*scene, "Body", "Left");
    auto right = usd_skel_test::MeshNamed(*scene, "Body", "Right");
    ASSERT_TRUE(left && right);
    const auto animated_joint = left.Get<asset::SkinnedMesh>().joints[0];
    const auto player         = animated_joint.Get<asset::AnimationTarget>().player;
    EXPECT_DOUBLE_EQ(player.Get<asset::AnimationPlayer>().clip->GetDuration(), 1.0);
    EXPECT_FALSE(right.Get<asset::SkinnedMesh>().joints[0].Has<asset::AnimationTarget>());
    const auto rest = right.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2];
    scene->GetWorld().Update(jobs, 0.5);
    EXPECT_EQ(right.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2], rest);
    EXPECT_NE(left.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2], rest);
    EXPECT_THROW(CookEditorScene(*scene), std::runtime_error);
    EXPECT_NO_THROW(CookEditorScene(*scene, {.scene_state = SceneCookState::Current}));
}

TEST(UsdSkelImportTest, RejectsUnsupportedSkinningAndInvalidInfluences) {
    core::JobSystem jobs;
    for (const auto& [from, to] : std::array<std::pair<std::string_view, std::string_view>, 5>{
             {{"uniform token[] skel:joints = [\"Root/Tip\", \"Root\"]", "uniform token primvars:skel:skinningMethod = \"dualQuaternion\""},
              {"jointIndices = [1,1,0,0]", "jointIndices = [9,1,0,0]"},
              {"jointWeights = [1,1,1,1]", "jointWeights = [0,1,1,1]"},
              {"jointWeights = [1,1,1,1]", "jointWeights = [-1,1,1,1]"},
              {"double3 xformOp:translate = (-1.5,0,0)", "double3 xformOp:translate.timeSamples = {24: (-1.5,0,0), 48: (0,0,0)}"}}}) {
        auto source = usd_skel_test::Source();
        usd_skel_test::Replace(source, from, to);
        const auto path = usd_skel_test::Write("usd-skel-invalid.usda", source);
        EXPECT_THROW(UsdSceneImporter{}.Import(jobs, path), std::runtime_error) << to;
    }
}

TEST(UsdSkelImportTest, SharedClipKeepsIndependentPlayersAndUnweightedMeshStaysStatic) {
    core::JobSystem jobs;
    auto source = usd_skel_test::Source();
    usd_skel_test::Replace(source, "rel skel:animationSource = </SwingRight>", "rel skel:animationSource = </SwingLeft>");
    usd_skel_test::Replace(source, "int[] primvars:skel:jointIndices = [0] (interpolation = \"constant\"; elementSize = 1)", "");
    usd_skel_test::Replace(source, "float[] primvars:skel:jointWeights = [1] (interpolation = \"constant\"; elementSize = 1)", "");
    const auto scene = UsdSceneImporter{}.Import(jobs, usd_skel_test::Write("usd-skel-shared-clip.usda", source));
    ASSERT_TRUE(scene);
    auto left  = usd_skel_test::MeshNamed(*scene, "Body", "Left");
    auto right = usd_skel_test::MeshNamed(*scene, "Body", "Right");
    auto badge = usd_skel_test::MeshNamed(*scene, "Badge", "Left");
    ASSERT_TRUE(left && right && badge);
    EXPECT_FALSE(badge.Has<asset::SkinnedMesh>());
    auto left_player  = left.Get<asset::SkinnedMesh>().joints[0].Get<asset::AnimationTarget>().player;
    auto right_player = right.Get<asset::SkinnedMesh>().joints[0].Get<asset::AnimationTarget>().player;
    EXPECT_NE(left_player, right_player);
    EXPECT_EQ(left_player.Get<asset::AnimationPlayer>().clip, right_player.Get<asset::AnimationPlayer>().clip);
    right_player.Get<asset::AnimationPlayer>().playing = false;
    scene->GetWorld().Update(jobs, 0.5);
    EXPECT_DOUBLE_EQ(left_player.Get<asset::AnimationPlayer>().time, 0.5);
    EXPECT_DOUBLE_EQ(right_player.Get<asset::AnimationPlayer>().time, 0.0);
    EXPECT_NE(left.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2],
              right.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>()[2]);
}

TEST(UsdSkelImportTest, CookedRoundTripRetainsDifferentClipsAndSharedMeshBindings) {
    core::FileIOManager files;
    core::JobSystem     jobs;
    asset::AssetManager manager(files, jobs, "assets");
    auto                source = UsdSceneImporter{}.Import(jobs, "examples/usd-viewer/assets/skinned.usda");
    const auto          cooked = CookEditorScene(*source);
    const auto          path   = std::filesystem::path("temp/htgc/skinned.hcscene");
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(cooked.GetData()), cooked.GetDataSize());
    }
    auto loaded = manager.ImportScene(path);
    auto left   = usd_skel_test::MeshNamed(*loaded, "Body", "Left");
    auto right  = usd_skel_test::MeshNamed(*loaded, "Body", "Right");
    ASSERT_TRUE(left && right);
    EXPECT_EQ(left.Get<asset::MeshComponent>().mesh, right.Get<asset::MeshComponent>().mesh);
    EXPECT_EQ(left.Get<asset::SkinnedMesh>().skin, right.Get<asset::SkinnedMesh>().skin);
    auto left_player  = left.Get<asset::SkinnedMesh>().joints[0].Get<asset::AnimationTarget>().player;
    auto right_player = right.Get<asset::SkinnedMesh>().joints[0].Get<asset::AnimationTarget>().player;
    EXPECT_NE(left_player.Get<asset::AnimationPlayer>().clip, right_player.Get<asset::AnimationPlayer>().clip);
    for (double dt : {0.0, .125, .25, .375, .8}) {
        source->GetWorld().Update(jobs, dt);
        loaded->GetWorld().Update(jobs, dt);
        for (auto original : source->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>()) {
            auto restored = usd_skel_test::MeshNamed(*loaded, original.Get<asset::MetaInfo>().name,
                                                     original.Get<asset::RelationShip>().parent.Get<asset::MetaInfo>().name);
            ASSERT_TRUE(restored);
            auto a = original.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>();
            auto b = restored.Get<asset::SkinnedMesh>().deformed_vertices->Span<asset::VertexAttribute::Position>();
            ASSERT_EQ(a.size(), b.size());
            for (unsigned i = 0; i < a.size(); ++i)
                for (unsigned k = 0; k < 3; ++k) EXPECT_NEAR(a[i][k], b[i][k], 1e-5f);
        }
    }
}
