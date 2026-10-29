#include "test_macros.hpp"
import interop.gtest;

import std;
import asset;
import core;
import ecs;
import gfx;
import math;
import render;

using namespace hitagi;
using namespace hitagi::asset;

namespace animation_test {
// These asset tests intentionally inspect CPU vertices. Rendering tests exercise
// the default GPU path independently.
void UpdateCpuReference(ecs::World& world, core::JobSystem& jobs, double dt = 0);
}

namespace animation_test {

auto TranslationClip(float distance = 2.0f) -> std::shared_ptr<AnimationClip> {
    AnimationTrack track{.target = 7};
    track.positions.keys = {{0.0, math::vec3f(0.0f)}, {1.0, math::vec3f(distance, 0.0f, 0.0f)}};
    return std::make_shared<AnimationClip>(1.0, std::vector{track});
}

auto TriangleVertices() -> VertexArray::Data {
    VertexArray::Data vertex_data;
    {
        vertex_data[VertexAttribute::Position] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Position>));
        auto p                                 = vertex_data[VertexAttribute::Position].Span<VertexDataType<VertexAttribute::Position>>();
        p[0]                                   = {-0.5f, -0.5f, 0.5f};
        p[1]                                   = {0.5f, -0.5f, 0.5f};
        p[2]                                   = {0.5f, 0.5f, 0.5f};
    }
    {
        vertex_data[VertexAttribute::Normal] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::Normal>));
        auto p                               = vertex_data[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();
        std::ranges::fill(p, math::vec3f(0, 0, 1));
    }
    {
        vertex_data[VertexAttribute::UV0] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::UV0>));
        auto p                            = vertex_data[VertexAttribute::UV0].Span<VertexDataType<VertexAttribute::UV0>>();
        p[0]                              = {0, 0};
        p[1]                              = {1, 0};
        p[2]                              = {1, 1};
    }
    {
        vertex_data[VertexAttribute::BlendIndex] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::BlendIndex>));
        auto values                              = vertex_data[VertexAttribute::BlendIndex].Span<VertexDataType<VertexAttribute::BlendIndex>>();
        std::ranges::fill(values, math::vec4u(0));
    }
    {
        vertex_data[VertexAttribute::BlendWeight] = core::Buffer(3 * sizeof(VertexDataType<VertexAttribute::BlendWeight>));
        auto values                               = vertex_data[VertexAttribute::BlendWeight].Span<VertexDataType<VertexAttribute::BlendWeight>>();
        std::ranges::fill(values, math::vec4f(1.0f, 0.0f, 0.0f, 0.0f));
    }
    return vertex_data;
}

auto WeightedTriangle(VertexArray::Data vertex_data = TriangleVertices()) -> std::shared_ptr<Mesh> {
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    core::Buffer                       index_data(std::as_bytes(std::span(indices)));
    return std::make_shared<Mesh>(std::make_shared<VertexArray>(3, std::move(vertex_data)), std::make_shared<IndexArray>(IndexType::UINT16, std::move(index_data)), std::pmr::vector<Mesh::SubMesh>{{.index_count = 3}});
}

auto Joint(Scene& scene, ecs::Entity player, ecs::Entity parent = {}) -> ecs::Entity {
    auto joint = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip(parent), asset::MetaInfo("Joint"));
    joint.Emplace<AnimationTarget>(player, 7);
    return joint;
}

auto Bind(Scene& scene, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Skin>& skin,
          ecs::Entity joint, ecs::Entity parent = {}) -> ecs::Entity {
    auto entity = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip(parent), asset::MetaInfo("Skin instance"), asset::MeshComponent{mesh});
    entity.Emplace<SkinnedMesh>(skin, std::vector{joint});
    return entity;
}

}  // namespace animation_test

class SkeletalAnimationTest : public testing::Test {
protected:
    Scene           scene{"AnimationTest"};
    core::JobSystem jobs{2};
    void            Update(double seconds) { animation_test::UpdateCpuReference(scene.GetWorld(), jobs, seconds); }
};

TEST_F(SkeletalAnimationTest, SharedMeshIndependentPlayersAndSharedJoints) {
    const auto mesh = animation_test::WeightedTriangle();
    const auto skin = std::make_shared<Skin>(std::vector{math::mat4f::identity()});
    const auto clip = animation_test::TranslationClip();
    auto       a    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(10.0f, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("A"));
    auto       b    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(-10.0f, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("B"));
    a.Emplace<AnimationPlayer>(clip);
    b.Emplace<AnimationPlayer>(clip, 0.0, 0.5);
    const auto joint_a  = animation_test::Joint(scene, a, a);
    const auto joint_b  = animation_test::Joint(scene, b, b);
    auto       body_a   = animation_test::Bind(scene, mesh, skin, joint_a, a);
    const auto body_b   = animation_test::Bind(scene, mesh, skin, joint_b, b);
    const auto clothing = animation_test::Bind(scene, mesh, skin, joint_a, a);
    EXPECT_THROW(render::pbr::RenderDrawItem::FromEntity(body_a), std::logic_error);
    Update(0.5);

    const auto draw_a   = render::pbr::RenderDrawItem::FromEntity(body_a);
    const auto draw_b   = render::deferred::RenderDrawItem::FromEntity(body_b, 42);
    const auto original = mesh->vertices->Span<VertexAttribute::Position>()[0];
    EXPECT_EQ(draw_a.mesh, draw_b.mesh);
    EXPECT_NE(draw_a.deformed_vertices, draw_b.deformed_vertices);
    EXPECT_NEAR(draw_a.deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 1.0f, 1e-5f);
    EXPECT_NEAR(draw_b.deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 0.5f, 1e-5f);
    EXPECT_EQ(draw_a.deformed_vertices->Span<VertexAttribute::Position>()[0],
              clothing.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0]);
    EXPECT_NEAR(joint_a.Get<Transform>().world_matrix[0][3], 11.0f, 1e-5f);
    EXPECT_EQ(draw_b.object_id, 42u);

    // Cached schedule must consume this frame's duration. Previously extracted
    // frames keep their own vertex snapshots while the next pose is evaluated.
    Update(0.25);
    EXPECT_DOUBLE_EQ(a.Get<AnimationPlayer>().time, 0.75);
    EXPECT_NEAR(body_a.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 1.5f, 1e-5f);
    EXPECT_NEAR(draw_a.deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 1.0f, 1e-5f);
    EXPECT_EQ(mesh->vertices->Span<VertexAttribute::Position>()[0], original);
    b.Get<AnimationPlayer>().clip = animation_test::TranslationClip(-4.0f);
    Update(0.0);
    EXPECT_NEAR(body_b.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x - 1.5f, 1e-5f);
    EXPECT_NEAR(body_a.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 1.5f, 1e-5f);
}

TEST_F(SkeletalAnimationTest, PlaybackLoopReversePauseSeekAndClipReplacement) {
    auto player = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Player"));
    player.Emplace<AnimationPlayer>(animation_test::TranslationClip());
    const auto joint = animation_test::Joint(scene, player);
    Update(1.25);
    EXPECT_DOUBLE_EQ(player.Get<AnimationPlayer>().time, 0.25);
    EXPECT_NEAR(joint.Get<Transform>().position.x, 0.5f, 1e-6f);
    player.Get<AnimationPlayer>().speed = -1.0;
    Update(0.5);
    EXPECT_DOUBLE_EQ(player.Get<AnimationPlayer>().time, 0.75);
    player.Get<AnimationPlayer>().playing = false;
    player.Get<AnimationPlayer>().time    = 0.125;
    Update(0.8);
    EXPECT_NEAR(joint.Get<Transform>().position.x, 0.25f, 1e-6f);
    player.Get<AnimationPlayer>().clip = animation_test::TranslationClip(4.0f);
    Update(0.0);
    EXPECT_NEAR(joint.Get<Transform>().position.x, 0.5f, 1e-6f);
    player.Get<AnimationPlayer>().playing = true;
    player.Get<AnimationPlayer>().looping = false;
    Update(2.0);
    EXPECT_DOUBLE_EQ(player.Get<AnimationPlayer>().time, 0.0);
    player.Get<AnimationPlayer>().speed = 1.0;
    Update(2.0);
    EXPECT_DOUBLE_EQ(player.Get<AnimationPlayer>().time, 1.0);
    EXPECT_NEAR(joint.Get<Transform>().position.x, 4.0f, 1e-6f);
}

TEST_F(SkeletalAnimationTest, SlerpStepAndUnauthoredChannels) {
    AnimationTrack track{.target = 7};
    track.rotations.keys       = {{0.0, math::quatf::identity()}, {1.0, math::quatf(0.0f, 0.0f, 1.0f, 0.0f)}};
    track.scales.interpolation = AnimationInterpolation::Step;
    track.scales.keys          = {{0.0, math::vec3f(1.0f)}, {0.5, math::vec3f(2.0f)}};
    auto player                = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Player"));
    player.Emplace<AnimationPlayer>(std::make_shared<AnimationClip>(1.0, std::vector{track}));
    auto joint                      = animation_test::Joint(scene, player);
    joint.Get<Transform>().position = math::vec3f(3.0f, 4.0f, 5.0f);
    Update(0.25);
    EXPECT_EQ(joint.Get<Transform>().scaling, math::vec3f(1.0f));
    Update(0.25);
    EXPECT_EQ(joint.Get<Transform>().scaling, math::vec3f(2.0f));
    EXPECT_NEAR(joint.Get<Transform>().rotation.z, std::sqrt(0.5f), 1e-5f);
    EXPECT_NEAR(joint.Get<Transform>().rotation.w, std::sqrt(0.5f), 1e-5f);
    EXPECT_EQ(joint.Get<Transform>().position, math::vec3f(3.0f, 4.0f, 5.0f));
    scene.GetWorld().GetEntityManager().Destroy(player);
    EXPECT_NO_THROW(Update(0.1));  // Stale player handle is ignored in its live World.
}

TEST_F(SkeletalAnimationTest, InverseBindMeshSpaceWeightsAndNormals) {
    auto vertex_data = animation_test::TriangleVertices();
    {
        auto values = vertex_data[VertexAttribute::BlendWeight].Span<VertexDataType<VertexAttribute::BlendWeight>>();

        std::ranges::fill(values, math::vec4f(2.0f, 0.0f, 0.0f, 0.0f));
        values[1] = math::vec4f(0.0f);
    }
    {
        auto values = vertex_data[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();
        std::ranges::fill(values, math::normalize(math::vec3f(1.0f, 1.0f, 0.0f)));
    }
    auto mesh                            = animation_test::WeightedTriangle(std::move(vertex_data));
    auto joint                           = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(5.0f, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("Joint"));
    auto skin                            = std::make_shared<Skin>(std::vector{math::translate(math::vec3f(-5.0f, 0.0f, 0.0f))});
    auto instance                        = animation_test::Bind(scene, mesh, skin, joint);
    instance.Get<Transform>().position.x = 3.0f;
    Update(0.0);
    const auto positions = mesh->vertices->Span<VertexAttribute::Position>();
    const auto output    = instance.Get<SkinnedMesh>().deformed_vertices;
    EXPECT_NEAR(output->Span<VertexAttribute::Position>()[0].x, positions[0].x - 3.0f, 1e-5f);
    EXPECT_EQ(output->Span<VertexAttribute::Position>()[1], positions[1]);
    joint.Get<Transform>().scaling = math::vec3f(2.0f, 1.0f, 1.0f);
    Update(0.0);
    const auto normal = instance.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Normal>()[0];
    EXPECT_NEAR(normal.x, 1.0f / std::sqrt(5.0f), 1e-5f);
    EXPECT_NEAR(normal.y, 2.0f / std::sqrt(5.0f), 1e-5f);
    joint.Get<Transform>().scaling = math::vec3f(-2.0f, 1.0f, 1.0f);
    Update(0.0);
    const auto reflected = instance.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Normal>()[0];
    EXPECT_NEAR(reflected.x, -1.0f / std::sqrt(5.0f), 1e-5f);
    EXPECT_NEAR(reflected.y, 2.0f / std::sqrt(5.0f), 1e-5f);
    scene.GetWorld().GetEntityManager().Destroy(joint);
    EXPECT_THROW(Update(0.0), std::invalid_argument);
    EXPECT_FALSE(instance.Get<SkinnedMesh>().deformed_vertices);
    const auto replacement                = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Replacement"));
    instance.Get<SkinnedMesh>().joints[0] = replacement;
    EXPECT_NO_THROW(Update(0.0));
}

TEST_F(SkeletalAnimationTest, JointHierarchyBlendsInfluencesAndDrivesAttachments) {
    auto vertex_data = animation_test::TriangleVertices();
    {
        auto values = vertex_data[VertexAttribute::BlendIndex].Span<VertexDataType<VertexAttribute::BlendIndex>>();
        std::ranges::fill(values, math::vec4u(0, 1, 999, 999));
    }
    {
        auto values = vertex_data[VertexAttribute::BlendWeight].Span<VertexDataType<VertexAttribute::BlendWeight>>();
        std::ranges::fill(values, math::vec4f(0.25f, 0.75f, 0.0f, 0.0f));
    }
    auto       mesh       = animation_test::WeightedTriangle(std::move(vertex_data));
    auto       root       = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(1.0f, 0.0f, 0.0f))), asset::RelationShip{}, asset::MetaInfo("Root joint"));
    auto       child      = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(2.0f, 0.0f, 0.0f))), asset::RelationShip(root), asset::MetaInfo("Child joint"));
    const auto attachment = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::translate(math::vec3f(0.2f, 0.0f, 0.0f))), asset::RelationShip(child), asset::MetaInfo("Attachment"));
    const auto skin       = std::make_shared<Skin>(std::vector{
        math::translate(math::vec3f(-1.0f, 0.0f, 0.0f)), math::translate(math::vec3f(-3.0f, 0.0f, 0.0f))});
    auto       instance   = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Mesh"), asset::MeshComponent{mesh});
    instance.Emplace<SkinnedMesh>(skin, std::vector{root, child});
    Update(0.0);
    const auto original = mesh->vertices->Span<VertexAttribute::Position>()[0];
    EXPECT_EQ(instance.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0], original);
    child.Get<Transform>().position.x = 4.0f;
    Update(0.0);
    EXPECT_NEAR(instance.Get<SkinnedMesh>().deformed_vertices->Span<VertexAttribute::Position>()[0].x, original.x + 1.5f, 1e-5f);
    EXPECT_NEAR(attachment.Get<Transform>().world_matrix[0][3], 5.2f, 1e-5f);
}

TEST(AnimationValidationTest, RejectsInvalidAssetsAndFrameTime) {
    AnimationTrack track{.target = 7};
    track.positions.keys = {{0.5, math::vec3f(0.0f)}, {0.5, math::vec3f(1.0f)}};
    EXPECT_THROW(AnimationClip(1.0, std::vector{track}), std::invalid_argument);
    track.positions.keys.clear();
    track.rotations.keys = {{0.0, math::quatf(0.0f)}};
    EXPECT_THROW(AnimationClip(1.0, std::vector{track}), std::invalid_argument);
    EXPECT_THROW(Skin(std::vector{math::mat4f::zero()}), std::invalid_argument);
    Scene           scene;
    core::JobSystem jobs;
    EXPECT_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs, -1.0), std::invalid_argument);
    EXPECT_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_NO_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs, 0.0));
}

TEST(AnimationValidationTest, RejectsInvalidPaletteAndWeights) {
    Scene           scene;
    core::JobSystem jobs;
    const auto      mesh     = animation_test::WeightedTriangle();
    const auto skin     = std::make_shared<Skin>(std::vector{math::mat4f::identity()});
    const auto      joint    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Joint"));
    auto       instance = animation_test::Bind(scene, mesh, skin, joint);
    instance.Get<SkinnedMesh>().joints.clear();
    EXPECT_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs), std::invalid_argument);
    instance.Get<SkinnedMesh>().joints.emplace_back(joint);
    auto vertex_data                                                   = animation_test::TriangleVertices();
    vertex_data[VertexAttribute::BlendIndex].Span<math::vec4u>()[0][0] = 1;
    instance.Get<MeshComponent>().mesh                                 = animation_test::WeightedTriangle(vertex_data);
    EXPECT_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs), std::invalid_argument);
    vertex_data[VertexAttribute::BlendIndex].Span<math::vec4u>()[0][0]  = 0;
    vertex_data[VertexAttribute::BlendWeight].Span<math::vec4f>()[0][0] = -1.0f;
    instance.Get<MeshComponent>().mesh                                  = animation_test::WeightedTriangle(vertex_data);
    EXPECT_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs), std::invalid_argument);
    vertex_data[VertexAttribute::BlendWeight].Span<math::vec4f>()[0][0] = 1.0f;
    instance.Get<MeshComponent>().mesh                                  = animation_test::WeightedTriangle(std::move(vertex_data));
    EXPECT_NO_THROW(animation_test::UpdateCpuReference(scene.GetWorld(), jobs));
}

TEST(SkinningResidencyTest, SceneLoadsReplacementSnapshotAndUnloadsCurrentStreams) {
    auto                      device = gfx::create_device(gfx::Device::Type::Mock);
    gfx::CommandQueues        queues(*device);
    auto                      bindings = gfx::BindlessUtils::Create(*device);
    gfx::ShaderCompiler       compiler("Skinning residency");
    const ResourceLoadContext context{*device, queues, *bindings, compiler};
    core::JobSystem           jobs;
    Scene                     scene;
    const auto                mesh     = animation_test::WeightedTriangle();
    const auto                skin     = std::make_shared<Skin>(std::vector{math::mat4f::identity()});
    auto                      joint    = scene.GetWorld().GetEntityManager().Create(asset::Transform(math::mat4f::identity()), asset::RelationShip{}, asset::MetaInfo("Joint"));
    const auto                instance = animation_test::Bind(scene, mesh, skin, joint);
    animation_test::UpdateCpuReference(scene.GetWorld(), jobs);
    scene.Load(context);
    const auto previous = instance.Get<SkinnedMesh>().deformed_vertices;
    EXPECT_EQ(previous->GetLoadState(), ResourceLoadState::Loaded);
    joint.Get<Transform>().position.x = 1.0f;
    animation_test::UpdateCpuReference(scene.GetWorld(), jobs);
    const auto current = instance.Get<SkinnedMesh>().deformed_vertices;
    EXPECT_NE(previous.get(), current.get());
    EXPECT_EQ(current->GetLoadState(), ResourceLoadState::Unloaded);
    scene.Load(context);
    EXPECT_EQ(current->GetLoadState(), ResourceLoadState::Loaded);
    scene.Unload();
    EXPECT_EQ(current->GetLoadState(), ResourceLoadState::Unloaded);
    EXPECT_FALSE(current->GetAttributeData(VertexAttribute::Position)->get().gpu_buffer);
}

TEST(AnimationValidationTest, AcceptsAffineRoundingButRejectsProjectiveSkin) {
    auto matrix = math::mat4f::identity();
    matrix[3][3] = std::nextafter(1.0f, 2.0f);
    EXPECT_NO_THROW(Skin(std::vector{matrix}));
    matrix[3][0] = 0.01f;
    EXPECT_THROW(Skin(std::vector{matrix}), std::invalid_argument);
    matrix[3][0] = std::numeric_limits<float>::infinity();
    EXPECT_THROW(Skin(std::vector{matrix}), std::invalid_argument);
}

TEST_F(SkeletalAnimationTest, PaletteCacheRespectsMeshSpaceAndBindingsAcrossFrames) {
    const auto mesh = animation_test::WeightedTriangle();
    const auto skin = std::make_shared<Skin>(std::vector{math::mat4f::identity()});
    auto root = scene.GetWorld().GetEntityManager().Create(Transform{}, RelationShip{}, MetaInfo("root"));
    const auto joint = animation_test::Joint(scene, root);
    auto a = animation_test::Bind(scene, mesh, skin, joint, root);
    auto shared = animation_test::Bind(scene, mesh, skin, joint, root);
    auto offset = animation_test::Bind(scene, mesh, skin, joint, root);
    offset.Get<Transform>().position = {3, 0, 0};
    scene.GetWorld().Update(jobs, 0);
    const auto old = a.Get<SkinnedMesh>().pose;
    const auto check = [&] {
        for (auto entity : {a, shared, offset}) {
            const auto& instance = entity.Get<SkinnedMesh>();
            const auto expected = math::inverse(entity.Get<Transform>().world_matrix) * joint.Get<Transform>().world_matrix;
            for (unsigned r = 0; r < 4; ++r)
                for (unsigned c = 0; c < 4; ++c) EXPECT_NEAR(instance.pose->matrices[0][r][c], expected[r][c], 1e-5f);
        }
    };
    check();
    auto mutable_joint = joint;
    mutable_joint.Get<Transform>().position.x += 2;
    scene.GetWorld().Update(jobs, 0);
    check();
    EXPECT_NE(old, a.Get<SkinnedMesh>().pose);
    EXPECT_NEAR(a.Get<SkinnedMesh>().pose->matrices[0][0][3] - old->matrices[0][0][3], 2, 1e-5f);
}

namespace animation_test {
// MSVC 14.51 misidentifies imported shared_ptr specializations when the
// SkinnedMesh concept checks here precede their use in the tests above.
void UpdateCpuReference(ecs::World& world, core::JobSystem& jobs, double dt) {
    for (auto entity : world.GetEntityManager().GetEntities<asset::SkinnedMesh>())
        entity.Get<asset::SkinnedMesh>().cpu_deformation = true;
    world.Update(jobs, dt);
}
}
