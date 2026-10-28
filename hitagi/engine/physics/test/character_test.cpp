#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import ecs;
import asset;
import math;
import physics;

using namespace hitagi;

class CharacterPhysicsTest : public testing::Test {
protected:
    asset::Scene scene{"CharacterPhysicsTest"};
    core::JobSystem jobs{2};
    ecs::Entity owner;

    void SetUp() override {
        scene.GetWorld().RegisterSystem<physics::CharacterPhysicsSystem>();
        owner = scene.GetWorld().GetEntityManager().Create(
            asset::Transform(math::mat4f::identity()), asset::RelationShip{}, physics::CharacterPhysics{});
        Rig().gravity = math::vec3f(0, -1, 0);
        Rig().bodies.push_back({.mode = 1, .shape = 0, .size = math::vec3f(0.1f),
                               .bind = math::translate(math::vec3f(0, 5, 0)), .mass = 1.0f});
    }
    auto Rig() -> physics::CharacterPhysics& { return owner.Get<physics::CharacterPhysics>(); }
    void Update(double seconds) { scene.GetWorld().Update(jobs, seconds); }
    auto Height() -> float { return (Rig().world->Pose(0) * math::vec4f(0, 0, 0, 1)).y; }
    void AddAnimation(bool looping) {
        auto clip = std::make_shared<asset::AnimationClip>(1.0, std::vector<asset::AnimationTrack>{});
        owner.Emplace<asset::AnimationPlayer>(asset::AnimationPlayer{.clip = clip, .looping = looping});
    }
};

TEST_F(CharacterPhysicsTest, SlowFramesPreserveWorldAndBoundCatchupWithoutBacklog) {
    Update(0);
    const auto world = Rig().world;
    const float before = Height();
    Update(10.0);
    EXPECT_EQ(Rig().world, world);
    EXPECT_LT(Height(), before);
    EXPECT_GT(Height(), before - 0.2f);
    EXPECT_GE(Rig().remainder, 0.0);
    EXPECT_LT(Rig().remainder, 1.0 / 120.0);
    const float after = Height();
    Update(1.0 / 120.0);
    EXPECT_EQ(Rig().world, world);
    EXPECT_GT(Height(), after - 0.03f);
    for (unsigned i = 0; i < 5; ++i) Update(0.4);
    EXPECT_EQ(Rig().world, world);
}

TEST_F(CharacterPhysicsTest, ExplicitResetReproducesWarmStart) {
    Update(0);
    const auto world = Rig().world;
    const float initial = Height();
    Update(1.0 / 60.0);
    Rig().reset = true;
    Update(0);
    EXPECT_NE(Rig().world, world);
    EXPECT_NEAR(Height(), initial, 1e-5f);
}

TEST_F(CharacterPhysicsTest, NonLoopingClipEndDoesNotRepeatedlyResetPhysics) {
    AddAnimation(false);
    Update(0);
    const auto world = Rig().world;
    for (unsigned i = 0; i < 8; ++i) Update(0.4);
    EXPECT_EQ(owner.Get<asset::AnimationPlayer>().time, 1.0);
    EXPECT_EQ(Rig().world, world);
}

TEST_F(CharacterPhysicsTest, PausePreservesPoseAndSeekResetsWorld) {
    AddAnimation(false);
    Update(0);
    Update(1.0 / 60.0);
    owner.Get<asset::AnimationPlayer>().playing = false;
    const auto world = Rig().world;
    const float paused = Height();
    Update(0.8);
    EXPECT_EQ(Rig().world, world);
    EXPECT_FLOAT_EQ(Height(), paused);
    owner.Get<asset::AnimationPlayer>().time = 0.5;
    Update(0);
    EXPECT_NE(Rig().world, world);
}

TEST_F(CharacterPhysicsTest, LoopDiscontinuityStillResetsWorld) {
    AddAnimation(true);
    Update(0);
    Update(0.75);
    const auto world = Rig().world;
    Update(0.5);
    EXPECT_NE(Rig().world, world);
    EXPECT_NEAR(owner.Get<asset::AnimationPlayer>().time, 0.25, 1e-8);
}
