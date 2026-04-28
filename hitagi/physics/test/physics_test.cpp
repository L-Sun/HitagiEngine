#include "test_macros.hpp"

import math;
import physics;

using namespace hitagi;
using namespace hitagi::physics;

TEST(PhysicsWorldTest, DynamicBodyFallsUnderGravity) {
    PhysicsWorld world({
        .name    = "PhysicsWorldFallTest",
        .gravity = math::vec3f{0.0f, -9.81f, 0.0f},
    });

    BodyDesc body_desc;
    body_desc.shape              = ShapeDesc::Box(math::vec3f{0.5f, 0.5f, 0.5f});
    body_desc.motion_type        = MotionType::Dynamic;
    body_desc.transform.position = math::vec3f{0.0f, 10.0f, 0.0f};

    const auto body_id = world.CreateAndAddBody(body_desc);

    ASSERT_TRUE(body_id.IsValid());
    EXPECT_TRUE(world.IsBodyAdded(body_id));
    EXPECT_EQ(world.GetNumBodies(), 1u);

    for (int i = 0; i < 60; ++i) {
        EXPECT_EQ(world.Step(1.0f / 60.0f), PhysicsUpdateError::None);
    }

    const auto transform = world.GetBodyTransform(body_id);
    EXPECT_LT(transform.position.y, 10.0f);

    world.RemoveAndDestroyBody(body_id);
    EXPECT_EQ(world.GetNumBodies(), 0u);
}
