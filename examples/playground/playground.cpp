#include "playground.hpp"
#include <hitagi/asset/mesh_factory.hpp>

#include <spdlog/logger.h>

using namespace hitagi::asset;
using namespace hitagi::math;

Playground::Playground(hitagi::Engine& engine)
    : RuntimeModule("Playground"),
      engine(engine),
      scene("playground"),
      camera(std::make_shared<CameraNode>(std::make_shared<Camera>(Camera::Parameters{}, "camera"))) {
    scene.camera_nodes.emplace_back(camera);

    auto cube  = scene.instance_nodes.emplace_back(std::make_shared<MeshNode>(MeshFactory::Cube()));
    auto light = scene.light_nodes.emplace_back(
        std::make_shared<LightNode>(
            std::make_shared<Light>(Light::Parameters{}, "light")));

    light->transform.local_translation = vec3f(2, -2, 2);

    cube->GetObjectRef()->sub_meshes.front().material_instance = hitagi::asset_manager->GetMaterial("Phong")->CreateInstance();
}

void Playground::Tick() {
    scene.Update();

    auto window_rect = engine.App().GetWindowsRect();
    auto viewport    = camera->GetObjectRef()->GetViewPort(
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top);

    engine.Renderer().RenderScene(scene, viewport, *camera);
}