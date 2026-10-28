#include "interop/gtest_macros.hpp"
import interop.gtest;
import std;
import engine;
import editor;

using namespace hitagi;

TEST(CharacterPhysicsAssetTest, LocalMikuTimingAndFinitePlayback) {
    const auto path = std::filesystem::path("temp/runtime-physics/usd/miku_bullet_physics.usda");
    if (!std::filesystem::is_regular_file(path)) GTEST_SKIP() << "Local third-party Miku assets not installed";
    const auto   start          = std::chrono::steady_clock::now();
    core::JobSystem jobs;
    auto            scene = UsdSceneImporter{}.Import(jobs, path);
    const double initialization = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    ecs::Entity  owner;
    for (auto entity : scene->GetWorld().GetEntityManager().GetEntities())
        if (entity.Has<physics::CharacterPhysics>()) owner = entity;
    ASSERT_TRUE(owner);
    auto& rig   = owner.Get<physics::CharacterPhysics>();
    rig.reset   = true;
    EXPECT_EQ(rig.bodies.size(), 173u);
    EXPECT_EQ(rig.constraints.size(), 229u);
    std::vector<double> total, physics;
    float               penetration     = 0;
    float               peak_tip_height = -std::numeric_limits<float>::infinity();
    int                 contact_a = -1, contact_b = -1;
    for (unsigned frame = 0; frame < 600; ++frame) {
        const auto begin = std::chrono::steady_clock::now();
        scene->GetWorld().Update(jobs, 1.0 / 60);
        total.emplace_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
        physics.emplace_back(rig.update_ms);
        // Body indices are part of this optional, fixed local Miku fixture.
        // Catch the asymmetric-limit sign regression across the complete clip.
        const auto body_height = [&](unsigned body) {
            return (rig.world->Pose(body) * math::vec4f(0, 0, 0, 1)).y;
        };
        peak_tip_height = std::max({peak_tip_height, body_height(55) - body_height(4), body_height(75) - body_height(4)});
        if (rig.penetration > penetration) {
            penetration        = rig.penetration;
            const auto contact = rig.world->DeepestContact();
            contact_a          = contact.body_a;
            contact_b          = contact.body_b;
        }
        for (auto joint : rig.joints) {
            const auto& transform = joint.Get<asset::Transform>();
            EXPECT_TRUE(std::isfinite(transform.position.norm()));
            EXPECT_NEAR(transform.rotation.norm(), 1.0f, 1e-4f);
        }
    }
    EXPECT_LT(peak_tip_height, 0.0f);
    std::filesystem::create_directories("temp/runtime-physics");
    std::ranges::sort(total);
    std::ranges::sort(physics);
    std::ofstream report("temp/runtime-physics/jolt-timing.json");
    report << std::format(
        R"({{"frames":600,"initialization_ms":{},"physics_binding_ms":{{"p50":{},"p95":{},"max":{}}},"scene_ms":{{"p50":{},"p95":{},"max":{}}},"peak_contact_penetration_rig_units":{}}})",
        initialization, physics[300], physics[570], physics.back(), total[300], total[570], total.back(), penetration);
    std::println("Physics+binding p95 {} ms; Scene p95 {} ms; contact penetration {} rig units",
                 physics[570], total[570], penetration);
    std::println("Peak contact body pair: {} / {}", contact_a, contact_b);

}
