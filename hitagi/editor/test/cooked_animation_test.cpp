#include "interop/gtest_macros.hpp"
import std;
import interop.gtest;
import engine;
import editor;

using namespace hitagi;
using namespace hitagi::asset;
using namespace hitagi::math;

TEST(CookedPoseProducerTest, RejectsUnencodedRuntimeBehavior) {
    Scene scene;
    auto& entities = scene.GetWorld().GetEntityManager();
    auto actor = entities.Create(Transform{}, RelationShip{});
    auto joint = entities.Create(Transform{}, RelationShip(actor));
    const std::array slots{JointSlot{}};
    auto layout = std::make_shared<JointLayout>(slots);
    const std::array joints{joint};
    actor.Emplace<AnimationPose>(JointBinding(layout, joints, actor));
    EXPECT_THROW(CookEditorScene(scene), std::runtime_error);
    actor.Remove<AnimationPose>();
    actor.Emplace<RigInstance>(std::make_shared<RigDefinition>(layout, RigDesc{}));
    EXPECT_THROW(CookEditorScene(scene), std::runtime_error);
}

namespace cooked_animation_test {
// These asset tests intentionally inspect CPU vertices. Rendering tests exercise
// the default GPU path independently.
void UpdateCpuReference(ecs::World& world, core::JobSystem& jobs, double dt = 0);
}

namespace cooked_animation_test {
template <typename T>
auto Read(const core::Buffer& bytes, std::uint64_t offset) -> T {
    T result;
    std::memcpy(&result, bytes.GetData() + offset, sizeof(T));
    return result;
}
template <typename T>
void Write(core::Buffer& bytes, std::uint64_t offset, const T& value) {
    std::memcpy(bytes.GetData() + offset, &value, sizeof(T));
}
auto Find(Scene& scene, std::string_view name) -> ecs::Entity {
    for (auto entity : scene.GetWorld().GetEntityManager().GetEntities())
        if (entity.Has<MetaInfo>() && entity.Get<MetaInfo>().name == name) return entity;
    throw std::runtime_error("Missing test entity");
}
auto Fixture(bool invalid_influence = false, bool unweighted = false) -> std::shared_ptr<Scene> {
    auto              scene = std::make_shared<Scene>("animation");
    VertexArray::Data vertex_data;
    core::Buffer      index_data(3 * sizeof(std::uint16_t));
    {
        vertex_data[VertexAttribute::Position] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Position>));
        auto p                                 = vertex_data[VertexAttribute::Position].Span<VertexDataType<VertexAttribute::Position>>();
        p[0]                                   = {0, 0, 0};
        p[1]                                   = {1, 0, 0};
        p[2]                                   = {0, 1, 0};
    }
    {
        vertex_data[VertexAttribute::Normal] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Normal>));
        auto p                               = vertex_data[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();
        std::ranges::fill(p, vec3f(0, 0, 1));
    }
    {
        vertex_data[VertexAttribute::BlendIndex] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::BlendIndex>));
        auto p                                   = vertex_data[VertexAttribute::BlendIndex].Span<VertexDataType<VertexAttribute::BlendIndex>>();
        p[0]                                     = {0, 1, 0, 0};
        p[1]                                     = {1, 0, 0, 0};
        p[2]                                     = {0, 1, 0, 0};
    }
    {
        vertex_data[VertexAttribute::BlendWeight] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::BlendWeight>));
        auto p                                    = vertex_data[VertexAttribute::BlendWeight].Span<VertexDataType<VertexAttribute::BlendWeight>>();
        std::ranges::fill(p, vec4f(.75f, .25f, 0, 0));
    }
    {
        auto p = index_data.Span<IndexDataType<IndexType::UINT16>>();
        p[0]   = 0;
        p[1]   = 1;
        p[2]   = 2;
    }
    if (invalid_influence) vertex_data[VertexAttribute::BlendIndex].Span<vec4u>()[0][0] = 7;
    if (unweighted) {
        vertex_data[VertexAttribute::BlendIndex].Span<vec4u>()[0]  = vec4u(0xFFFFFFFFu);
        vertex_data[VertexAttribute::BlendWeight].Span<vec4f>()[0] = vec4f(0);
    }
    const auto mesh     = std::make_shared<Mesh>(std::make_shared<VertexArray>(3, std::move(vertex_data)), std::make_shared<IndexArray>(IndexType::UINT16, std::move(index_data)), std::pmr::vector<Mesh::SubMesh>{{.index_count = 3}}, "shared mesh");
    auto       affine   = mat4f::identity();
    affine[0]           = {-2, .25f, 0, 1};
    affine[1]           = {0, 3, 0, -2};
    affine[2]           = {0, 0, .5f, 4};
    auto           skin = std::make_shared<Skin>(std::vector{affine, math::translate(vec3f(0, -1, 0))}, "shared skin");
    AnimationTrack root{.target = 41}, tip{.target = 7};
    root.positions.keys         = {{0, {0, 0, 0}}, {.125, {.5f, 0, 0}}, {.375, {1, 0, 0}}, {1, {0, 0, 0}}};
    root.rotations.keys         = {{0, quatf::identity()}, {.125, math::axis_angle_to_quaternion(vec3f(0, 0, 1), .2f)}, {.375, math::axis_angle_to_quaternion(vec3f(0, 0, 1), .4f)}, {1, quatf::identity()}};
    root.scales.keys            = {{0, vec3f(1)}, {.125, vec3f(1)}, {.375, vec3f(1)}, {1, vec3f(1)}};
    tip.positions.interpolation = AnimationInterpolation::Step;
    tip.positions.keys          = {{0, {0, 1, 0}}, {.25, {0, 2, 0}}, {.5, {0, 3, 0}}, {.75, {0, 1, 0}}};
    auto clip                   = std::make_shared<AnimationClip>(1, std::vector{root, tip}, "shared clip");
    for (unsigned i = 0; i < 2; ++i) {
        auto group = scene->GetWorld().GetEntityManager().Create(asset::Transform(math::translate(vec3f(float(i) * 3, 0, 0))), asset::RelationShip{}, asset::MetaInfo(std::format("group{}", i)));
        group.Emplace<AnimationPlayer>(clip, 0.0, i == 0 ? 1.0 : -.5, true, i == 0);
        auto joint = scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip(group), asset::MetaInfo(std::format("joint{}", i)));
        joint.Emplace<AnimationTarget>(group, 41);
        auto tip_joint = scene->GetWorld().GetEntityManager().Create(asset::Transform(math::translate(vec3f(0, 1, 0))), asset::RelationShip(joint), asset::MetaInfo(std::format("tip{}", i)));
        tip_joint.Emplace<AnimationTarget>(group, 7);
        auto instance = scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip(group), asset::MetaInfo(std::format("mesh{}", i)), asset::MeshComponent{mesh});
        instance.Emplace<SkinnedMesh>(skin, std::vector{tip_joint, joint});
    }
    auto empty                      = scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("signed scale"));
    empty.Get<Transform>().scaling  = {-2, 3, .5f};
    empty.Get<Transform>().rotation = math::axis_angle_to_quaternion(vec3f(0, 1, 0), .8f);
    (void)scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip(empty), asset::MetaInfo("camera"), asset::CameraComponent{});
    (void)scene->GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip(empty), asset::MetaInfo("light"), asset::LightComponent{});
    return scene;
}
void Compare(Scene& a, Scene& b) {
    for (auto source : a.GetWorld().GetEntityManager().GetEntities()) {
        auto        target = Find(b, source.Get<MetaInfo>().name);
        const auto& x      = source.Get<Transform>().world_matrix;
        const auto& y      = target.Get<Transform>().world_matrix;
        for (unsigned r = 0; r < 4; ++r)
            for (unsigned c = 0; c < 4; ++c) EXPECT_NEAR(x[r][c], y[r][c], 2e-5f);
        if (source.Has<SkinnedMesh>()) {
            auto p = source.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>();
            auto q = target.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>();
            ASSERT_EQ(p.size(), q.size());
            for (unsigned i = 0; i < p.size(); ++i)
                for (unsigned k = 0; k < 3; ++k) EXPECT_NEAR(p[i][k], q[i][k], 2e-5f);
        }
    }
}
}  // namespace cooked_animation_test
using namespace cooked_animation_test;
class CookedAnimationTest : public testing::Test {
protected:
    core::IOService files;
    core::JobSystem jobs;
    AssetService    manager{files, jobs, "assets"};
};

TEST_F(CookedAnimationTest, SharedAssetsIndependentInstancesAndHierarchyRoundTrip) {
    auto source = Fixture();
    auto cooked = CookEditorScene(*source);
    // Cooking must not tick systems or generate/rewrite deformation snapshots.
    EXPECT_FALSE(Find(*source, "mesh0").Get<SkinnedMesh>().deformed_vertices);
    const auto again = CookEditorScene(*source);
    EXPECT_TRUE(std::ranges::equal(cooked.Span<const std::byte>(), again.Span<const std::byte>()));
    auto loaded = manager.LoadCookedScene(cooked);
    auto left = Find(*loaded, "mesh0"), right = Find(*loaded, "mesh1");
    EXPECT_EQ(left.Get<MeshComponent>().mesh, right.Get<MeshComponent>().mesh);
    EXPECT_EQ(left.Get<SkinnedMesh>().skin, right.Get<SkinnedMesh>().skin);
    EXPECT_NE(left.Get<SkinnedMesh>().joints[0], right.Get<SkinnedMesh>().joints[0]);
    EXPECT_EQ(left.Get<SkinnedMesh>().joints[0].Get<MetaInfo>().name, "tip0");
    EXPECT_EQ(left.Get<SkinnedMesh>().joints[1].Get<AnimationTarget>().target, 41u);
    EXPECT_EQ(Find(*loaded, "group0").Get<AnimationPlayer>().clip, Find(*loaded, "group1").Get<AnimationPlayer>().clip);
    EXPECT_EQ(Find(*loaded, "signed scale").Get<Transform>().scaling, vec3f(-2, 3, .5f));
    EXPECT_EQ(Find(*loaded, "camera").Get<RelationShip>().parent, Find(*loaded, "signed scale"));
    const auto original = Find(*source, "mesh0").Get<SkinnedMesh>().skin->GetInverseBindMatrices();
    const auto restored = left.Get<SkinnedMesh>().skin->GetInverseBindMatrices();
    EXPECT_TRUE(std::ranges::equal(std::as_bytes(original), std::as_bytes(restored)));
    for (double dt : {0.0, .125, .2, .3, .8}) {
        cooked_animation_test::UpdateCpuReference(source->GetWorld(), jobs, dt);
        cooked_animation_test::UpdateCpuReference(loaded->GetWorld(), jobs, dt);
        Compare(*source, *loaded);
    }
    EXPECT_NE(left.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0], right.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0]);
    EXPECT_THROW(CookEditorScene(*source), std::runtime_error);
    const auto time     = Find(*source, "group0").Get<AnimationPlayer>().time;
    const auto snapshot = Find(*source, "mesh0").Get<SkinnedMesh>().deformed_vertices;
    auto       captured = manager.LoadCookedScene(CookEditorScene(*source, {.scene_state = SceneCookState::Current}));
    EXPECT_EQ(Find(*source, "mesh0").Get<SkinnedMesh>().deformed_vertices, snapshot);
    EXPECT_DOUBLE_EQ(Find(*source, "group0").Get<AnimationPlayer>().time, time);
    cooked_animation_test::UpdateCpuReference(captured->GetWorld(), jobs);
    Compare(*source, *captured);
    Find(*loaded, "group1").Get<AnimationPlayer>().playing = true;
    Find(*source, "group1").Get<AnimationPlayer>().playing = true;
    cooked_animation_test::UpdateCpuReference(source->GetWorld(), jobs, .2);
    cooked_animation_test::UpdateCpuReference(loaded->GetWorld(), jobs, .2);
    Compare(*source, *loaded);
}

TEST_F(CookedAnimationTest, CompactAffineSparseChannelsAndSharedTimeArrays) {
    auto       source = Fixture();
    auto       data   = CookEditorScene(*source);
    const auto header = Read<CookedFileHeader>(data, 0);
    const auto scene  = Read<SceneRecord>(data, header.meta.offset + header.root_offset);
    EXPECT_EQ(header.version, 8u);
    EXPECT_EQ(scene.skins.count, 1u);
    EXPECT_EQ(scene.clips.count, 1u);
    EXPECT_EQ(scene.meshes.count, 1u);
    const auto skin = Read<SkinRecord>(data, header.meta.offset + scene.skins.offset);
    EXPECT_EQ(skin.inverse_bind_matrices.size, 2u * 48u);
    const auto        clip = Read<AnimationClipRecord>(data, header.meta.offset + scene.clips.offset);
    asset::BufferView position_times{}, rotation_times{};
    for (std::uint64_t i = 0; i < clip.tracks.count; ++i) {
        const auto track = Read<AnimationTrackRecord>(data, header.meta.offset + clip.tracks.offset + i * sizeof(AnimationTrackRecord));
        if (track.target == 7) EXPECT_EQ(track.curves.count, 1u);
        for (std::uint64_t j = 0; j < track.curves.count; ++j) {
            const auto curve = Read<CurveRecord>(data, header.meta.offset + track.curves.offset + j * sizeof(CurveRecord));
            if (track.target == 7) {
                EXPECT_EQ(curve.time_encoding, CurveTimeEncoding::Uniform);
                EXPECT_EQ(curve.times.size, 0u);
                EXPECT_DOUBLE_EQ(curve.step, .25);
            } else if (curve.property == CurveProperty::Scale) {
                EXPECT_EQ(curve.key_count, 1u);
                EXPECT_EQ(curve.values.size, 12u);
                EXPECT_EQ(curve.times.size, 0u);
            } else {
                EXPECT_EQ(curve.time_encoding, CurveTimeEncoding::Explicit);
                EXPECT_EQ(curve.times.size, 4u * sizeof(double));
                if (curve.property == CurveProperty::Translation) {
                    position_times = curve.times;
                    EXPECT_EQ(curve.values.size, 4u * 12u);
                } else {
                    rotation_times = curve.times;
                    EXPECT_EQ(curve.values.size, 4u * 16u);
                }
            }
        }
    }
    EXPECT_EQ(position_times.offset, rotation_times.offset);
    EXPECT_NE(position_times.size, 0u);
}

TEST_F(CookedAnimationTest, AnimationOnlyNodesForwardReferencesAndMissingChannels) {
    Scene source("no meshes");
    auto  child                      = source.GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("child"));
    child.Get<Transform>().position  = {3, 4, 5};
    auto parent                      = source.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(vec3f(10, 0, 0))), asset::RelationShip{}, asset::MetaInfo("parent"));
    child.Get<RelationShip>().parent = parent;  // deliberately forward reference, no scene Update
    AnimationTrack track{.target = 123};
    track.scales.keys = {{.25, vec3f(1)}, {.75, vec3f(2)}};
    auto clip         = std::make_shared<AnimationClip>(1, std::vector{track});
    parent.Emplace<AnimationPlayer>(clip);
    child.Emplace<AnimationTarget>(parent, 123);
    auto loaded = manager.LoadCookedScene(CookEditorScene(source));
    EXPECT_TRUE(loaded->GetWorld().GetEntityManager().GetEntities<asset::MeshComponent>().empty());
    cooked_animation_test::UpdateCpuReference(source.GetWorld(), jobs, .5);
    cooked_animation_test::UpdateCpuReference(loaded->GetWorld(), jobs, .5);
    Compare(source, *loaded);
    EXPECT_EQ(Find(*loaded, "child").Get<Transform>().position, vec3f(3, 4, 5));
}

TEST_F(CookedAnimationTest, RejectsInvalidBindingsAndCorruptRecords) {
    auto       source  = Fixture();
    auto       data    = CookEditorScene(*source);
    const auto header  = Read<CookedFileHeader>(data, 0);
    const auto root    = Read<SceneRecord>(data, header.meta.offset + header.root_offset);
    auto       corrupt = data;
    auto       old     = header;
    old.version        = 4;
    Write(corrupt, 0, old);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt           = data;
    auto scene        = root;
    scene.nodes.count = std::numeric_limits<std::uint64_t>::max();
    Write(corrupt, header.meta.offset + header.root_offset, scene);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt     = data;
    auto node   = Read<NodeRecord>(data, header.meta.offset + root.nodes.offset + sizeof(NodeRecord));
    node.parent = 1;
    Write(corrupt, header.meta.offset + root.nodes.offset + sizeof(NodeRecord), node);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt            = data;
    auto target        = Read<AnimationTargetRecord>(data, header.meta.offset + root.targets.offset);
    target.player_node = std::numeric_limits<std::uint32_t>::max();
    Write(corrupt, header.meta.offset + root.targets.offset, target);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt           = data;
    auto skin         = Read<SkinnedMeshRecord>(data, header.meta.offset + root.skinned_meshes.offset);
    skin.joints.count = 0;
    Write(corrupt, header.meta.offset + root.skinned_meshes.offset, skin);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt             = data;
    const auto clip     = Read<AnimationClipRecord>(data, header.meta.offset + root.clips.offset);
    const auto track    = Read<AnimationTrackRecord>(data, header.meta.offset + clip.tracks.offset);
    const auto offset   = header.meta.offset + track.curves.offset;
    auto       curve    = Read<CurveRecord>(data, offset);
    curve.time_encoding = static_cast<CurveTimeEncoding>(99);
    Write(corrupt, offset, curve);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt = data;
    curve   = Read<CurveRecord>(data, offset);
    curve.values.size -= 1;
    Write(corrupt, offset, curve);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt    = data;
    curve      = Read<CurveRecord>(data, offset);
    curve.step = 0;
    Write(corrupt, offset, curve);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    corrupt         = data;
    curve           = Read<CurveRecord>(data, offset);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Write(corrupt, header.blob.offset + curve.values.offset, nan);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::invalid_argument);
    corrupt               = data;
    const auto skin_asset = Read<SkinRecord>(data, header.meta.offset + root.skins.offset);
    Write(corrupt, header.blob.offset + skin_asset.inverse_bind_matrices.offset, nan);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::invalid_argument);
    corrupt     = data;
    auto player = Read<AnimationPlayerRecord>(data, header.meta.offset + root.players.offset);
    player.clip = 99;
    Write(corrupt, header.meta.offset + root.players.offset, player);
    EXPECT_THROW(manager.LoadCookedScene(corrupt), std::runtime_error);
    Scene external;
    auto  alien                                         = external.GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("external"));
    Find(*source, "mesh0").Get<SkinnedMesh>().joints[0] = alien;
    EXPECT_THROW(CookEditorScene(*source), std::runtime_error);
    source                                             = Fixture();
    Find(*source, "group0").Get<RelationShip>().parent = Find(*source, "joint0");
    EXPECT_THROW(CookEditorScene(*source), std::runtime_error);
}

TEST_F(CookedAnimationTest, SourceMatrixAndInfluenceValidationDoesNotLoseInvalidData) {
    auto perspective  = mat4f::identity();
    perspective[3][0] = .125f;
    EXPECT_THROW((void)std::make_shared<Skin>(std::vector{perspective}), std::invalid_argument);
    auto source = Fixture(true);
    EXPECT_THROW(CookEditorScene(*source), std::runtime_error);
    source                                           = Fixture();
    Find(*source, "mesh0").Get<MeshComponent>().mesh = std::make_shared<Mesh>();
    EXPECT_THROW(CookEditorScene(*source), std::runtime_error);
}

TEST_F(CookedAnimationTest, UnweightedVerticesAndDormantPlayersRemainValid) {
    auto source = Fixture(false, true);
    Find(*source, "group0").Get<AnimationPlayer>().clip.reset();
    auto loaded = manager.LoadCookedScene(CookEditorScene(*source));
    EXPECT_FALSE(Find(*loaded, "group0").Get<AnimationPlayer>().clip);
    cooked_animation_test::UpdateCpuReference(source->GetWorld(), jobs, .3);
    cooked_animation_test::UpdateCpuReference(loaded->GetWorld(), jobs, .3);
    Compare(*source, *loaded);
    EXPECT_EQ(Find(*loaded, "mesh0").Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0], vec3f(0));
}

TEST_F(CookedAnimationTest, EmptySceneAndMultipleRootsRoundTrip) {
    Scene empty("empty");
    EXPECT_EQ(empty.GetWorld().GetEntityManager().NumEntities(), 0u);
    auto loaded_empty = manager.LoadCookedScene(CookEditorScene(empty));
    EXPECT_EQ(loaded_empty->GetWorld().GetEntityManager().NumEntities(), 0u);

    auto& entities = empty.GetWorld().GetEntityManager();
    auto  left     = entities.Create(Transform(vec3f(2, 0, 0)), RelationShip{}, MetaInfo("left"));
    auto  right    = entities.Create(Transform(vec3f(9, 0, 0)), RelationShip{}, MetaInfo("right"));
    auto  child    = entities.Create(Transform(vec3f(0, 3, 0)), RelationShip(left), MetaInfo("child"));
    auto  loaded   = manager.LoadCookedScene(CookEditorScene(empty));
    EXPECT_EQ(loaded->GetWorld().GetEntityManager().NumEntities(), 3u);
    EXPECT_FALSE(Find(*loaded, "left").Get<RelationShip>().parent);
    EXPECT_FALSE(Find(*loaded, "right").Get<RelationShip>().parent);
    EXPECT_EQ(Find(*loaded, "child").Get<RelationShip>().parent, Find(*loaded, "left"));
    EXPECT_NEAR(Find(*loaded, "child").Get<Transform>().world_matrix[0][3], 2.0f, 1e-5f);

    // Direct ECS reparenting must be cooked without relying on cached child lists.
    child.Get<RelationShip>().parent = right;
    loaded                           = manager.LoadCookedScene(CookEditorScene(empty));
    EXPECT_EQ(Find(*loaded, "child").Get<RelationShip>().parent, Find(*loaded, "right"));
    EXPECT_NEAR(Find(*loaded, "child").Get<Transform>().world_matrix[0][3], 9.0f, 1e-5f);

    auto       bytes  = CookEditorScene(empty);
    const auto header = Read<CookedFileHeader>(bytes, 0);
    const auto record = Read<SceneRecord>(bytes, header.meta.offset + header.root_offset);
    auto       node   = Read<NodeRecord>(bytes, header.meta.offset + record.nodes.offset);
    node.parent       = static_cast<std::uint32_t>(record.nodes.count);
    Write(bytes, header.meta.offset + record.nodes.offset, node);
    EXPECT_THROW(manager.LoadCookedScene(bytes), std::runtime_error);
}

TEST_F(CookedAnimationTest, CurrentSnapshotPreservesMaterialOverridesAndComponentState) {
    auto scene    = Fixture();
    auto material = std::make_shared<Material>(MaterialParameters{{.name = "roughness", .value = 0.5f}});
    auto clone    = material->Clone("local material");
    clone->SetParameter("roughness", 0.8f);
    Find(*scene, "mesh0").Get<MeshComponent>().material_overrides = {material};
    Find(*scene, "mesh1").Get<MeshComponent>().material_overrides = {clone};
    Find(*scene, "camera").Get<CameraComponent>().near_clip       = 0.25f;
    Find(*scene, "camera").Get<Transform>().position              = {3, 4, 5};
    Find(*scene, "light").Get<LightComponent>().intensity         = 7;
    const auto  cooked                                            = CookEditorScene(*scene, {.scene_state = SceneCookState::Current});
    auto        restored                                          = manager.LoadCookedScene(cooked, "assets");
    const auto& a                                                 = Find(*restored, "mesh0").Get<MeshComponent>();
    const auto& b                                                 = Find(*restored, "mesh1").Get<MeshComponent>();
    EXPECT_EQ(a.mesh, b.mesh);
    ASSERT_EQ(a.material_overrides.size(), 1);
    ASSERT_EQ(b.material_overrides.size(), 1);
    EXPECT_NE(a.material_overrides[0], b.material_overrides[0]);
    EXPECT_FLOAT_EQ(a.material_overrides[0]->GetParameter<float>("roughness").value(), 0.5f);
    EXPECT_FLOAT_EQ(b.material_overrides[0]->GetParameter<float>("roughness").value(), 0.8f);
    EXPECT_FLOAT_EQ(Find(*restored, "camera").Get<CameraComponent>().near_clip, 0.25f);
    EXPECT_EQ(Find(*restored, "camera").Get<Transform>().position, vec3f(3, 4, 5));
    EXPECT_FLOAT_EQ(Find(*restored, "light").Get<LightComponent>().intensity, 7);
    auto& overrides = Find(*scene, "mesh0").Get<MeshComponent>().material_overrides;
    overrides       = {nullptr};
    auto fallback   = manager.LoadCookedScene(CookEditorScene(*scene), "assets");
    EXPECT_EQ(Find(*fallback, "mesh0").Get<MeshComponent>().material_overrides[0], nullptr);
    overrides = {material, clone};
    EXPECT_THROW(CookEditorScene(*scene), std::runtime_error);
}

namespace cooked_animation_test {
// Keep these concept checks after the tests to avoid MSVC 14.51 imported
// shared_ptr specialization mismatches (see animation_test.cpp).
void UpdateCpuReference(ecs::World& world, core::JobSystem& jobs, double dt) {
    for (auto entity : world.GetEntityManager().GetEntities<asset::SkinnedMesh>())
        entity.Get<asset::SkinnedMesh>().cpu_deformation = true;
    world.Update(jobs, dt);
}
}
