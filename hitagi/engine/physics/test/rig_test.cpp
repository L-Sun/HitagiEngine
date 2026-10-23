#include "test_macros.hpp"
import interop.gtest;
import std;
import math;
import physics;

using namespace hitagi;
using namespace hitagi::physics;

TEST(RigWorldTest, AuthoredJointAnchorsFallingBodyAndResetClearsHistory) {
    for (auto backend : {RigBackend::Bullet, RigBackend::Jolt}) {
        std::vector<RigBody> bodies{
            {.mode = 0, .shape = 0, .size = math::vec3f(0.1f), .bind = math::mat4f::identity()},
            {.mode = 1, .shape = 2, .size = math::vec3f(0.1f, 1.0f, 0.0f), .bind = math::translate(math::vec3f(0.0f, -0.5f, 0.0f)), .mass = 1.0f}};
        RigJoint joint{.a = 0, .b = 1};
        joint.angular_min = math::vec3f(-0.5f);
        joint.angular_max = math::vec3f(0.5f);
        RigWorld                 world(bodies, std::vector{joint}, math::vec3f(0, -9.81f, 0), backend);
        std::vector<math::mat4f> anchors{bodies[0].bind, bodies[1].bind};
        world.Reset(anchors);
        for (unsigned i = 0; i < 240; ++i) world.Step(anchors, 1.0f / 120);
        const math::vec3f p = (world.Pose(1) * math::vec4f(0, 0, 0, 1)).xyz;
        EXPECT_NEAR(p.y, -0.5f, 0.01f);
        anchors[0] = math::translate(math::vec3f(0.3f, 0, 0));
        for (unsigned i = 0; i < 240; ++i) world.Step(anchors, 1.0f / 120);
        const math::vec3f moved_anchor = (world.Pose(1) * math::vec4f(0, 0.5f, 0, 1)).xyz;
        EXPECT_NEAR(moved_anchor.x, 0.3f, 0.01f);
        EXPECT_NEAR(moved_anchor.y, 0.0f, 0.01f);
        anchors[0] = bodies[0].bind;
        world.Reset(anchors);
        for (unsigned i = 0; i < 240; ++i) world.Step(anchors, 1.0f / 120);
        const math::vec3f reset = (world.Pose(1) * math::vec4f(0, 0, 0, 1)).xyz;
        EXPECT_NEAR(reset.x, p.x, 1e-5f);
        EXPECT_NEAR(reset.y, p.y, 1e-5f);
    }
}

TEST(RigWorldTest, CollisionMasksPreventUnwantedContacts) {
    for (auto backend : {RigBackend::Bullet, RigBackend::Jolt}) {
        std::vector<RigBody> bodies{
            {.mode = 0, .shape = 1, .size = math::vec3f(5, 0.1f, 5), .mask = 0},
            {.mode = 1, .shape = 0, .size = math::vec3f(0.1f), .bind = math::translate(math::vec3f(0, 1, 0))}};
        RigWorld                 world(bodies, {}, math::vec3f(0, -9.81f, 0), backend);
        std::vector<math::mat4f> poses{bodies[0].bind, bodies[1].bind};
        world.Reset(poses);
        for (unsigned i = 0; i < 120; ++i) world.Step(poses, 1.0f / 120);
        const math::vec3f p = (world.Pose(1) * math::vec4f(0, 0, 0, 1)).xyz;
        EXPECT_LT(p.y, -2.0f);
        EXPECT_FLOAT_EQ(world.MaxPenetration(), 0.0f);
    }
}

TEST(RigWorldTest, EnabledContactSupportsFallingSphere) {
    for (auto backend : {RigBackend::Bullet, RigBackend::Jolt}) {
        std::vector<RigBody> bodies{
            {.mode = 0, .shape = 1, .size = math::vec3f(5, 0.1f, 5)},
            {.mode = 1, .shape = 0, .size = math::vec3f(0.1f), .bind = math::translate(math::vec3f(0, 1, 0))}};
        RigWorld                 world(bodies, {}, math::vec3f(0, -9.81f, 0), backend);
        std::vector<math::mat4f> poses{bodies[0].bind, bodies[1].bind};
        world.Reset(poses);
        for (unsigned i = 0; i < 240; ++i) world.Step(poses, 1.0f / 120);
        const math::vec3f p = (world.Pose(1) * math::vec4f(0, 0, 0, 1)).xyz;
        EXPECT_NEAR(p.y, 0.2f, backend == RigBackend::Jolt ? 0.021f : 0.005f);
        EXPECT_LT(world.MaxPenetration(), backend == RigBackend::Jolt ? 0.021f : 0.005f);
    }
}

TEST(RigWorldTest, EquilibriumSpringRestoresDisplacedFreeAxis) {
    for (auto backend : {RigBackend::Bullet, RigBackend::Jolt}) {
        std::vector<RigBody> bodies{
            {.mode = 0, .shape = 0, .size = math::vec3f(0.1f), .mask = 0},
            {.mode = 1, .shape = 0, .size = math::vec3f(0.1f), .mass = 1.0f, .mask = 0}};
        RigJoint joint{.a = 0, .b = 1};
        joint.linear_min    = math::vec3f(-2, 0, 0);
        joint.linear_max    = math::vec3f(2, 0, 0);
        joint.linear_spring = math::vec3f(4, 0, 0);
        RigWorld                 world(bodies, std::vector{joint}, math::vec3f(0), backend);
        std::vector<math::mat4f> poses{bodies[0].bind, math::translate(math::vec3f(1, 0, 0))};
        world.Reset(poses);
        for (unsigned i = 0; i < 30; ++i) world.Step(poses, 1.0f / 120);
        const math::vec3f p = (world.Pose(1) * math::vec4f(0, 0, 0, 1)).xyz;
        EXPECT_LT(p.x, 0.95f);
        EXPECT_GT(p.x, 0.5f);
        EXPECT_NEAR(p.y, 0.0f, 1e-5f);
    }
}

TEST(RigWorldTest, AsymmetricAngularLimitsPreserveAuthoredBulletConvention) {
    // Bullet Generic6Dof reports the negative physical angle for a pure axis
    // rotation of body B relative to A. Thus [-0.1, 0.4] permits [-0.4, 0.1].
    for (auto backend : {RigBackend::Bullet, RigBackend::Jolt}) {
        for (unsigned axis = 0; axis < 3; ++axis) {
            std::vector<RigBody> bodies{
                {.mode = 0, .shape = 0, .size = math::vec3f(0.5f), .mask = 0},
                {.mode = 1, .shape = 0, .size = math::vec3f(0.5f), .mass = 1.0f, .angular_damping = 0.99f, .mask = 0}};
            RigJoint joint{.a = 0, .b = 1};
            joint.angular_min[axis] = -0.1f;
            joint.angular_max[axis] = 0.4f;
            RigWorld    world(bodies, std::vector{joint}, math::vec3f(0), backend);
            math::quatf q(0, 0, 0, std::cos(0.3f));
            q[axis] = std::sin(0.3f);
            std::vector<math::mat4f> poses{bodies[0].bind, math::rotate(q)};
            world.Reset(poses);
            for (unsigned i = 0; i < 240; ++i) world.Step(poses, 1.0f / 120);
            const auto [position, rotation, scale] = math::decompose(world.Pose(1));
            const float angle                      = 2.0f * std::atan2(rotation[axis], rotation.w);
            EXPECT_LE(angle, 0.11f) << "backend=" << int(backend) << " axis=" << axis;
            EXPECT_GE(angle, -0.41f);
        }
    }
}
