#include "playground.hpp"
#include <hitagi/asset/mesh_factory.hpp>
#include <hitagi/hid/input_manager.hpp>

#include <spdlog/logger.h>

using namespace hitagi::asset;
using namespace hitagi::math;
using namespace hitagi::hid;

Playground::Playground(hitagi::Engine& engine)
    : RuntimeModule("Playground"),
      engine(engine),
      scene("playground"),
      camera(std::make_shared<CameraNode>(std::make_shared<Camera>(Camera::Parameters{}, "camera"))) {
    scene.camera_nodes.emplace_back(camera);
    camera->Attach(scene.root);

    cube = scene.instance_nodes.emplace_back(std::make_shared<MeshNode>(MeshFactory::Cube()));
    cube->Attach(scene.root);
    cube->transform.local_translation = vec3f(0.5, 0.5, 0.5);

    auto light = scene.light_nodes.emplace_back(std::make_shared<LightNode>(std::make_shared<Light>(Light::Parameters{}, "light")));
    light->Attach(scene.root);
    light->transform.local_translation = vec3f(2, -2, 2);

    cube->GetObjectRef()->sub_meshes.front().material_instance = hitagi::asset_manager->GetMaterial("Phong")->CreateInstance();
}

void Playground::Tick() {
    scene.Update();

    engine.GuiManager().DrawGui([&]() {
        static bool open = true;
        if (ImGui::Begin("Cube info", &open)) {
            ImGui::DragFloat3("Cube position", cube->transform.local_translation, 0.01);
            ImGui::DragFloat3("Cube scaling", cube->transform.local_scaling, 0.01);

            ImGui::Separator();

            ImGui::DragFloat3("Camera position", camera->transform.local_translation, 0.01);
            ImGui::DragFloat3("Camera eye", camera->GetObjectRef()->parameters.eye, 0.01);
        }
        ImGui::End();
    });

    auto window_rect = engine.App().GetWindowsRect();

    camera->GetObjectRef()->parameters.aspect = (window_rect.right - window_rect.left) / (window_rect.bottom - window_rect.top);

    auto& renderer = engine.Renderer();
    renderer.RenderGui(renderer.RenderScene(scene, *camera));
}