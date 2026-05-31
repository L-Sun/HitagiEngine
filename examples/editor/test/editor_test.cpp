#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

import editor;
import engine;

using namespace hitagi;

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    auto file_io_manager = std::make_unique<core::FileIOManager>();
    auto job_system      = std::make_unique<core::JobSystem>();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(EditorTest, ParsesLaunchOptions) {
    const char* argv[] = {
        "editor",
        "--open-scene",
        "assets/test/test.usda",
        "--frames",
        "12",
        "--screenshot",
        "temp/editor-smoke.png",
        "--hid-script",
        "temp/editor-input.json",
        "--hid-control",
        "temp/editor-control.jsonl",
        "--exit-after-load",
    };

    auto options = ParseEditorLaunchOptions(static_cast<int>(std::size(argv)), const_cast<char**>(argv));

    ASSERT_TRUE(options.open_scene);
    EXPECT_EQ(*options.open_scene, std::filesystem::path("assets/test/test.usda"));
    ASSERT_TRUE(options.frames);
    EXPECT_EQ(*options.frames, 12);
    ASSERT_TRUE(options.screenshot);
    EXPECT_EQ(*options.screenshot, std::filesystem::path("temp/editor-smoke.png"));
    ASSERT_TRUE(options.hid_script);
    EXPECT_EQ(*options.hid_script, std::filesystem::path("temp/editor-input.json"));
    ASSERT_TRUE(options.hid_control);
    EXPECT_EQ(*options.hid_control, std::filesystem::path("temp/editor-control.jsonl"));
    EXPECT_TRUE(options.exit_after_load);
}

TEST(EditorTest, CreatesFixtureScene) {
    auto scene = CreateEditorFixtureScene();

    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->GetRootEntity());
    EXPECT_GE(CountSceneEntities(*scene), 5);
    EXPECT_EQ(scene->GetCameraEntities().size(), 1);
    EXPECT_EQ(scene->GetLightEntities().size(), 1);
}

TEST(EditorTest, CreatesDefaultSceneWithCameraAtTwoTwoTwoLookingAtOrigin) {
    auto scene = CreateEditorDefaultScene();

    ASSERT_TRUE(scene);
    ASSERT_EQ(scene->GetCameraEntities().size(), 1);

    const auto camera_entity = scene->GetCameraEntities().front();
    ASSERT_TRUE(camera_entity.Has<asset::CameraComponent>());

    const auto camera = camera_entity.Get<asset::CameraComponent>().camera;
    ASSERT_TRUE(camera);

    EXPECT_FLOAT_EQ(camera->parameters.eye.x, 2.0f);
    EXPECT_FLOAT_EQ(camera->parameters.eye.y, 2.0f);
    EXPECT_FLOAT_EQ(camera->parameters.eye.z, 2.0f);

    const auto expected_look_dir = math::normalize(math::vec3f{-2.0f, -2.0f, -2.0f});
    EXPECT_NEAR(camera->parameters.look_dir.x, expected_look_dir.x, 1e-5f);
    EXPECT_NEAR(camera->parameters.look_dir.y, expected_look_dir.y, 1e-5f);
    EXPECT_NEAR(camera->parameters.look_dir.z, expected_look_dir.z, 1e-5f);
}

TEST(EditorTest, EditorStateTracksSceneSelectionAndDirtyState) {
    EditorState state;
    auto        scene = CreateEditorFixtureScene();
    auto        root  = scene->GetRootEntity();

    state.SetCurrentScene(scene);
    EXPECT_EQ(state.GetCurrentScene(), scene);

    state.SelectEntity(root);
    EXPECT_EQ(state.GetSelectedEntity(), root);

    state.MarkDirty();
    EXPECT_TRUE(state.IsDirty());
    state.MarkClean();
    EXPECT_FALSE(state.IsDirty());

    state.SetCurrentScene(nullptr);
    EXPECT_EQ(state.GetCurrentScene(), nullptr);
    EXPECT_FALSE(state.GetSelectedEntity());
}

TEST(EditorTest, ClassifiesEditorAssetsAndTracksSelection) {
    EXPECT_EQ(ClassifyEditorAssetPath("assets/test/test.usda"), EditorAssetKind::Scene);
    EXPECT_EQ(ClassifyEditorAssetPath("scene.hscene"), EditorAssetKind::Unknown);
    EXPECT_EQ(ClassifyEditorAssetPath("texture.PNG"), EditorAssetKind::Texture);
    EXPECT_EQ(ClassifyEditorAssetPath("materials/phong.json"), EditorAssetKind::Material);
    EXPECT_EQ(ClassifyEditorAssetPath("notes.txt"), EditorAssetKind::Unknown);

    EditorState state;
    state.SetSelectedAsset("assets/test/test.png", EditorAssetKind::Texture);
    EXPECT_EQ(state.GetSelectedAssetKind(), EditorAssetKind::Texture);
    EXPECT_EQ(state.GetSelectedAssetPath(), std::filesystem::path("assets/test/test.png"));
    state.ClearSelectedAsset();
    EXPECT_EQ(state.GetSelectedAssetKind(), EditorAssetKind::Unknown);
    EXPECT_TRUE(state.GetSelectedAssetPath().empty());
}

TEST(EditorTest, CommandStackUndoRedoTransformChange) {
    auto scene  = CreateEditorFixtureScene();
    auto entity = scene->CreateEmptyEntity(math::mat4f::identity(), scene->GetRootEntity(), "command-target");

    auto before      = entity.Get<asset::Transform>();
    auto after       = before;
    after.position.x = 7.0f;
    after.position.y = 8.0f;

    EditorCommandStack stack;
    stack.MarkClean();
    stack.Execute(std::make_unique<TransformChangeCommand>(entity, before, after));

    EXPECT_TRUE(stack.IsDirty());
    EXPECT_TRUE(stack.CanUndo());
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.x, 7.0f);
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.y, 8.0f);

    stack.Undo();
    EXPECT_FALSE(stack.IsDirty());
    EXPECT_TRUE(stack.CanRedo());
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.x, before.position.x);
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.y, before.position.y);

    stack.Redo();
    EXPECT_TRUE(stack.IsDirty());
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.x, 7.0f);
    EXPECT_FLOAT_EQ(entity.Get<asset::Transform>().position.y, 8.0f);
}

TEST(EditorTest, ComponentCommandsUndoRedoValues) {
    auto material_instance = std::make_shared<asset::MaterialInstance>(asset::MaterialParameters{
        {"roughness", 1.0f},
    });
    auto before_material = asset::MaterialParameter{.name = "roughness", .value = 1.0f};
    auto after_material  = asset::MaterialParameter{.name = "roughness", .value = 0.25f};

    EditorCommandStack stack;
    stack.Execute(std::make_unique<MaterialParameterChangeCommand>(material_instance, before_material, after_material));
    EXPECT_FLOAT_EQ(material_instance->GetParameter<float>("roughness").value(), 0.25f);
    stack.Undo();
    EXPECT_FLOAT_EQ(material_instance->GetParameter<float>("roughness").value(), 1.0f);

    auto camera       = std::make_shared<asset::Camera>(asset::Camera::Parameters{});
    auto camera_after = camera->parameters;
    camera_after.far_clip = 42.0f;
    stack.Execute(std::make_unique<CameraParameterChangeCommand>(camera, camera->parameters, camera_after));
    EXPECT_FLOAT_EQ(camera->parameters.far_clip, 42.0f);
    stack.Undo();
    EXPECT_FLOAT_EQ(camera->parameters.far_clip, asset::Camera::Parameters{}.far_clip);

    auto light       = std::make_shared<asset::Light>(asset::Light::Parameters{});
    auto light_after = light->parameters;
    light_after.intensity = 9.0f;
    stack.Execute(std::make_unique<LightParameterChangeCommand>(light, light->parameters, light_after));
    EXPECT_FLOAT_EQ(light->parameters.intensity, 9.0f);
    stack.Undo();
    EXPECT_FLOAT_EQ(light->parameters.intensity, asset::Light::Parameters{}.intensity);
}

TEST(EditorTest, ComponentAddRemoveCommandsUpdateSceneLists) {
    auto scene  = CreateEditorFixtureScene();
    auto entity = scene->CreateEmptyEntity(math::mat4f::identity(), scene->GetRootEntity(), "component-target");

    EditorCommandStack stack;
    const auto         initial_camera_count = scene->GetCameraEntities().size();
    const auto         initial_light_count  = scene->GetLightEntities().size();

    stack.Execute(std::make_unique<AddSceneComponentCommand>(*scene, entity, SceneComponentKind::Camera, "Added Camera"));
    ASSERT_TRUE(entity.Has<asset::CameraComponent>());
    EXPECT_EQ(scene->GetCameraEntities().size(), initial_camera_count + 1);
    stack.Undo();
    EXPECT_FALSE(entity.Has<asset::CameraComponent>());
    EXPECT_EQ(scene->GetCameraEntities().size(), initial_camera_count);
    stack.Redo();
    EXPECT_TRUE(entity.Has<asset::CameraComponent>());

    stack.Execute(std::make_unique<RemoveSceneComponentCommand>(*scene, entity, SceneComponentKind::Camera));
    EXPECT_FALSE(entity.Has<asset::CameraComponent>());
    EXPECT_EQ(scene->GetCameraEntities().size(), initial_camera_count);
    stack.Undo();
    EXPECT_TRUE(entity.Has<asset::CameraComponent>());
    EXPECT_EQ(scene->GetCameraEntities().size(), initial_camera_count + 1);

    stack.Execute(std::make_unique<AddSceneComponentCommand>(*scene, entity, SceneComponentKind::Light, "Added Light"));
    ASSERT_TRUE(entity.Has<asset::LightComponent>());
    EXPECT_EQ(scene->GetLightEntities().size(), initial_light_count + 1);
    stack.Undo();
    EXPECT_FALSE(entity.Has<asset::LightComponent>());
    EXPECT_EQ(scene->GetLightEntities().size(), initial_light_count);
    stack.Redo();
    EXPECT_TRUE(entity.Has<asset::LightComponent>());
}

TEST(EditorTest, RuntimeSceneCloneDoesNotMutateEditScene) {
    auto edit_scene = CreateEditorFixtureScene();
    auto runtime_scene = CreateEditorRuntimeScene(*edit_scene);

    ASSERT_TRUE(runtime_scene);
    ASSERT_NE(runtime_scene, edit_scene);
    EXPECT_EQ(CountSceneEntities(*runtime_scene), CountSceneEntities(*edit_scene));
    EXPECT_NE(runtime_scene->GetRootEntity(), edit_scene->GetRootEntity());

    auto edit_root_name = std::pmr::string{edit_scene->GetRootEntity().Get<asset::MetaInfo>().name};
    runtime_scene->RenameEntity(runtime_scene->GetRootEntity(), "runtime-root");
    runtime_scene->CreateEmptyEntity(math::mat4f::identity(), runtime_scene->GetRootEntity(), "runtime-only");
    runtime_scene->Update();

    EXPECT_EQ(edit_scene->GetRootEntity().Get<asset::MetaInfo>().name, edit_root_name);
    EXPECT_NE(CountSceneEntities(*runtime_scene), CountSceneEntities(*edit_scene));
}

TEST(EditorTest, EditorStateTracksPlayPauseModes) {
    EditorState state;
    EXPECT_EQ(state.GetMode(), EditorMode::Edit);
    state.SetMode(EditorMode::Play);
    EXPECT_EQ(state.GetMode(), EditorMode::Play);
    state.SetMode(EditorMode::Pause);
    EXPECT_EQ(state.GetMode(), EditorMode::Pause);
    state.SetMode(EditorMode::Edit);
    EXPECT_EQ(state.GetMode(), EditorMode::Edit);

    EXPECT_FALSE(state.IsSnapEnabled());
    state.SetSnapEnabled(true);
    state.SetTranslateSnap(0.5f);
    state.SetRotateSnap(30.0_deg);
    state.SetScaleSnap(0.25f);
    EXPECT_TRUE(state.IsSnapEnabled());
    EXPECT_FLOAT_EQ(state.GetTranslateSnap(), 0.5f);
    EXPECT_FLOAT_EQ(state.GetRotateSnap(), 30.0_deg);
    EXPECT_FLOAT_EQ(state.GetScaleSnap(), 0.25f);
}

namespace {
auto CreateEditorPickingTriangle() -> std::shared_ptr<asset::Mesh> {
    auto vertices = std::make_shared<asset::VertexArray>(3, "picking-triangle");
    vertices->Modify<asset::VertexAttribute::Position>([](std::span<math::vec3f> positions) {
        positions[0] = {0.0f, 0.0f, 0.0f};
        positions[1] = {1.0f, 0.0f, 0.0f};
        positions[2] = {0.0f, 0.0f, 1.0f};
    });

    auto indices = std::make_shared<asset::IndexArray>(3, asset::IndexType::UINT32, "picking-triangle");
    indices->Modify<asset::IndexType::UINT32>([](std::span<std::uint32_t> values) {
        values[0] = 0;
        values[1] = 1;
        values[2] = 2;
    });

    auto mesh = std::make_shared<asset::Mesh>(vertices, indices, "picking-triangle");
    mesh->AddSubMesh({
        .index_count  = 3,
        .index_offset = 0,
    });
    return mesh;
}
}  // namespace

TEST(EditorTest, ViewportPickingHitsNearestEntityMesh) {
    auto scene = std::make_shared<asset::Scene>("picking");
    auto near_entity = scene->CreateMeshEntity(asset::MeshFactory::Cube(), math::translate(math::vec3f{0.0f, 5.0f, 0.0f}), scene->GetRootEntity(), "near");
    scene->CreateMeshEntity(asset::MeshFactory::Cube(), math::translate(math::vec3f{0.0f, 9.0f, 0.0f}), scene->GetRootEntity(), "far");
    scene->Update();

    const auto pick = PickEditorEntity(
        *scene,
        EditorViewportRay{
            .origin    = {0.0f, 0.0f, 0.0f},
            .direction = {0.0f, 1.0f, 0.0f},
        });

    ASSERT_TRUE(pick);
    EXPECT_EQ(pick->entity, near_entity);
    EXPECT_GT(pick->distance, 0.0f);
}

TEST(EditorTest, ViewportPickingIgnoresMeshAABBMisses) {
    auto scene = std::make_shared<asset::Scene>("picking");
    scene->CreateMeshEntity(CreateEditorPickingTriangle(), math::translate(math::vec3f{0.0f, 5.0f, 0.0f}), scene->GetRootEntity(), "triangle");
    scene->Update();

    const auto pick = PickEditorEntity(
        *scene,
        EditorViewportRay{
            .origin    = {0.9f, 0.0f, 0.9f},
            .direction = {0.0f, 1.0f, 0.0f},
        });

    EXPECT_FALSE(pick);
}

TEST(EditorTest, ViewportRayUsesCameraProjectionCenter) {
    auto camera = asset::Camera(asset::Camera::Parameters{
        .aspect         = 1.0f,
        .near_clip      = 0.1f,
        .far_clip       = 100.0f,
        .horizontal_fov = 60.0_deg,
        .eye            = {0.0f, 0.0f, 0.0f},
        .look_dir       = {0.0f, 1.0f, 0.0f},
        .up             = {0.0f, 0.0f, 1.0f},
    });

    const auto ray = BuildEditorViewportRay(camera, math::mat4f::identity(), {50.0f, 50.0f}, {100.0f, 100.0f});

    EXPECT_NEAR(math::dot(ray.direction, math::vec3f{0.0f, 1.0f, 0.0f}), 1.0f, 1e-4f);
}

TEST(EditorTest, WorldXYGridStepDoesNotChangeWhenOnlyCameraYawChanges) {
    const auto camera_position = math::vec3f{2.0f, 2.0f, 2.0f};
    const auto first_step      = ComputeEditorWorldXYGridMinorStep(camera_position, 60.0_deg, 16.0f / 9.0f, 720.0f);

    for (const auto yaw_degrees : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
        const auto yaw       = yaw_degrees * std::numbers::pi_v<float> / 180.0f;
        const auto rotated_x = std::cos(yaw) * camera_position.x - std::sin(yaw) * camera_position.y;
        const auto rotated_y = std::sin(yaw) * camera_position.x + std::cos(yaw) * camera_position.y;
        const auto step      = ComputeEditorWorldXYGridMinorStep({rotated_x, rotated_y, camera_position.z}, 60.0_deg, 16.0f / 9.0f, 720.0f);

        EXPECT_FLOAT_EQ(step, first_step);
    }
}

TEST(EditorTest, ViewportNavigationDoesNothingWithoutMiddleMouseOrbit) {
    auto camera = asset::Camera::Parameters{
        .eye      = {0.0f, 0.0f, 0.0f},
        .look_dir = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f},
    };
    auto transform = asset::Transform{};
    auto state     = EditorViewportNavigationState{};

    const auto before_position = transform.position;
    const auto before_rotation = transform.rotation;

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .mouse_delta      = {80.0f, -20.0f},
            .viewport_hovered = true,
        });

    EXPECT_FALSE(state.orbiting);
    EXPECT_FLOAT_EQ(transform.position.x, before_position.x);
    EXPECT_FLOAT_EQ(transform.position.y, before_position.y);
    EXPECT_FLOAT_EQ(transform.position.z, before_position.z);
    EXPECT_FLOAT_EQ(transform.rotation.x, before_rotation.x);
    EXPECT_FLOAT_EQ(transform.rotation.y, before_rotation.y);
    EXPECT_FLOAT_EQ(transform.rotation.z, before_rotation.z);
    EXPECT_FLOAT_EQ(transform.rotation.w, before_rotation.w);
}

TEST(EditorTest, ViewportNavigationMmbOrbitsAroundFocusedPivot) {
    auto camera = asset::Camera::Parameters{
        .eye      = {0.0f, 0.0f, 0.0f},
        .look_dir = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f},
    };
    auto transform = asset::Transform{{0.0f, -5.0f, 0.0f}};
    auto state     = EditorViewportNavigationState{
        .orbit_pivot     = {0.0f, 0.0f, 0.0f},
        .has_orbit_pivot = true,
    };

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .viewport_hovered = true,
            .middle_down      = true,
        });

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .mouse_delta      = {80.0f, -20.0f},
            .viewport_hovered = true,
            .middle_down      = true,
        });

    const auto camera_basis = math::rotate(transform.rotation);
    const auto world_eye    = transform.position + math::vec3f{(camera_basis * math::vec4f(camera.eye, 0.0f)).xyz};
    const auto world_look   = math::normalize(math::vec3f{(camera_basis * math::vec4f(camera.look_dir, 0.0f)).xyz});

    EXPECT_TRUE(state.orbiting);
    EXPECT_NEAR((world_eye - state.orbit_pivot).norm(), 5.0f, 1e-4f);
    EXPECT_NEAR(math::dot(math::normalize(state.orbit_pivot - world_eye), world_look), 1.0f, 1e-4f);
}

TEST(EditorTest, ViewportNavigationMmbDefaultsToWorldOriginPivot) {
    auto camera = asset::Camera::Parameters{
        .eye      = {0.0f, 0.0f, 0.0f},
        .look_dir = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f},
    };
    auto transform = asset::Transform{{0.0f, -5.0f, 0.0f}};
    auto state     = EditorViewportNavigationState{};

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .viewport_hovered = true,
            .middle_down      = true,
        });

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .mouse_delta      = {40.0f, 0.0f},
            .viewport_hovered = true,
            .middle_down      = true,
        });

    const auto camera_basis = math::rotate(transform.rotation);
    const auto world_eye    = transform.position + math::vec3f{(camera_basis * math::vec4f(camera.eye, 0.0f)).xyz};

    EXPECT_TRUE(state.orbiting);
    EXPECT_NEAR((world_eye - math::vec3f{}).norm(), 5.0f, 1e-4f);
}

TEST(EditorTest, ViewportNavigationWheelZoomsTowardOrbitPivot) {
    auto camera = asset::Camera::Parameters{
        .eye      = {0.0f, 0.0f, 0.0f},
        .look_dir = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f},
    };
    auto transform = asset::Transform{{0.0f, -10.0f, 0.0f}};
    auto state     = EditorViewportNavigationState{};

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .scroll_delta     = {0.0f, 1.0f},
            .viewport_hovered = true,
        });

    const auto zoomed_in_distance = transform.position.norm();
    EXPECT_NEAR(zoomed_in_distance, 8.2f, 1e-4f);

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .scroll_delta     = {0.0f, -1.0f},
            .viewport_hovered = true,
        });

    EXPECT_NEAR(transform.position.norm(), 10.0f, 1e-4f);
}

TEST(EditorTest, ViewportNavigationBelowXYPlaneKeepsMiddleMouseRightScreenRight) {
    auto camera = asset::Camera::Parameters{
        .eye      = {0.0f, 0.0f, 0.0f},
        .look_dir = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f},
    };
    auto transform = asset::Transform{
        {0.0f, 0.0f, -5.0f},
        math::axis_angle_to_quaternion(math::vec3f{1.0f, 0.0f, 0.0f}, static_cast<float>(90.0_deg)),
    };
    auto state = EditorViewportNavigationState{
        .orbit_pivot     = {0.0f, 0.0f, 0.0f},
        .has_orbit_pivot = true,
    };

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .viewport_hovered = true,
            .middle_down      = true,
        });

    ApplyEditorViewportNavigation(
        camera,
        transform,
        state,
        EditorViewportNavigationInput{
            .delta_time       = 1.0f / 60.0f,
            .mouse_delta      = {80.0f, 0.0f},
            .viewport_hovered = true,
            .middle_down      = true,
        });

    const auto camera_basis = math::rotate(transform.rotation);
    const auto world_look   = math::normalize(math::vec3f{(camera_basis * math::vec4f(camera.look_dir, 0.0f)).xyz});
    EXPECT_GT(world_look.x, 0.0f);
}

TEST(EditorTest, HierarchyCommandsEditAndUndoRedo) {
    auto scene = CreateEditorFixtureScene();
    auto root  = scene->GetRootEntity();

    EditorState        state;
    EditorCommandStack stack;
    state.SetCurrentScene(scene);
    stack.MarkClean();

    const auto initial_count = CountSceneEntities(*scene);
    stack.Execute(std::make_unique<CreateEmptyEntityCommand>(*scene, root, "created", &state));
    auto created = state.GetSelectedEntity();
    ASSERT_TRUE(created);
    EXPECT_EQ(CountSceneEntities(*scene), initial_count + 1);
    EXPECT_EQ(created.Get<asset::MetaInfo>().name, "created");

    stack.Execute(std::make_unique<RenameEntityCommand>(*scene, created, "created", "renamed"));
    EXPECT_EQ(created.Get<asset::MetaInfo>().name, "renamed");
    stack.Undo();
    EXPECT_EQ(created.Get<asset::MetaInfo>().name, "created");
    stack.Redo();
    EXPECT_EQ(created.Get<asset::MetaInfo>().name, "renamed");

    auto new_parent = scene->CreateEmptyEntity(math::mat4f::identity(), root, "new-parent");
    scene->Update();
    stack.Execute(std::make_unique<ReparentEntityCommand>(*scene, created, root, new_parent));
    EXPECT_EQ(created.Get<asset::RelationShip>().parent, new_parent);
    stack.Undo();
    EXPECT_EQ(created.Get<asset::RelationShip>().parent, root);
    stack.Redo();
    EXPECT_EQ(created.Get<asset::RelationShip>().parent, new_parent);

    stack.Execute(std::make_unique<DuplicateEntitySubtreeCommand>(*scene, created, &state));
    EXPECT_EQ(CountSceneEntities(*scene), initial_count + 3);
    ASSERT_TRUE(state.GetSelectedEntity());
    EXPECT_EQ(state.GetSelectedEntity().Get<asset::MetaInfo>().name, "renamed Copy");
    stack.Undo();
    EXPECT_EQ(CountSceneEntities(*scene), initial_count + 2);

    stack.Execute(std::make_unique<DeleteEntitySubtreeCommand>(*scene, created, &state));
    EXPECT_EQ(CountSceneEntities(*scene), initial_count + 1);
    EXPECT_FALSE(state.GetSelectedEntity());
    stack.Undo();
    EXPECT_EQ(CountSceneEntities(*scene), initial_count + 2);
    ASSERT_TRUE(state.GetSelectedEntity());
    EXPECT_EQ(state.GetSelectedEntity().Get<asset::MetaInfo>().name, "renamed");
}

TEST(EditorTest, ImportsUsdSceneWithoutEditorWindow) {
    auto asset_manager = std::make_unique<asset::AssetManager>("assets");
    auto scene         = asset_manager->ImportScene("assets/test/test.usda");

    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->GetRootEntity());
    EXPECT_GE(CountSceneEntities(*scene), 1);
}
