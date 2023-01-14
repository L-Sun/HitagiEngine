#include <hitagi/engine.hpp>
#include <hitagi/asset/mesh_factory.hpp>

#include <array>

using namespace hitagi;

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;

    asset::Scene scene("playground");

    auto camera = std::make_shared<asset::CameraNode>(std::make_shared<asset::Camera>(asset::Camera::Parameters{}));
    auto light  = std::make_shared<asset::LightNode>(std::make_shared<asset::Light>(asset::Light::Parameters{}));
    auto cube   = std::make_shared<asset::MeshNode>(asset::MeshFactory::Cube());

    scene.camera_nodes.emplace_back(camera);
    scene.light_nodes.emplace_back(light);
    scene.instance_nodes.emplace_back(cube);

    camera->Attach(scene.root);
    light->Attach(scene.root);
    cube->Attach(scene.root);

    for (auto& submesh : cube->GetObjectRef()->sub_meshes) {
        submesh.material_instance = asset_manager->GetMaterial("Phong")->CreateInstance();
    }

    while (!engine.App().IsQuit()) {
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

        scene.Update();

        auto& renderer = engine.Renderer();
        renderer.RenderGui(renderer.RenderScene(scene, *camera));

        engine.Tick();
    }

    return 0;
}