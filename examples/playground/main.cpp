#include <hitagi/engine.hpp>
#include <hitagi/asset/mesh_factory.hpp>

#include <spdlog/spdlog.h>

using namespace hitagi;

#include <filesystem>
#include <shlobj.h>

static auto GetLatestWinPixGpuCapturerPath_Cpp17() {
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &programFilesPath);

    std::filesystem::path pixInstallationPath = programFilesPath;
    pixInstallationPath /= "Microsoft PIX";

    std::string newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath)) {
        if (directory_entry.is_directory()) {
            if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().string()) {
                newestVersionFound = directory_entry.path().filename().string();
            }
        }
    }

    if (newestVersionFound.empty()) {
        // TODO: Error, no PIX installation found
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

auto main(int argc, char** argv) -> int {
    if (GetModuleHandle("WinPixGpuCapturer.dll") == nullptr) {
        LoadLibrary(GetLatestWinPixGpuCapturerPath_Cpp17().string().c_str());
    }

    spdlog::set_level(spdlog::level::trace);

    hitagi::Engine engine;

    auto scene = std::make_shared<asset::Scene>("playground");

    auto camera = std::make_shared<asset::CameraNode>(std::make_shared<asset::Camera>(asset::Camera::Parameters{}));
    auto light  = std::make_shared<asset::LightNode>(std::make_shared<asset::Light>(asset::Light::Parameters{}));
    auto cube   = std::make_shared<asset::MeshNode>(asset::MeshFactory::Cube());

    scene->camera_nodes.emplace_back(camera);
    scene->light_nodes.emplace_back(light);
    scene->instance_nodes.emplace_back(cube);

    camera->Attach(scene->root);
    light->Attach(scene->root);
    cube->Attach(scene->root);

    for (auto& sub_mesh : cube->GetObjectRef()->sub_meshes) {
        sub_mesh.material_instance = asset::AssetManager::Get()->GetMaterial("Phong")->CreateInstance();
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

        scene->Update();

        auto& renderer      = engine.Renderer();
        auto  render_target = renderer.GetRenderGraph().Create(
            hitagi::gfx::TextureDesc{
                 .width       = renderer.GetSwapChain().GetWidth(),
                 .height      = renderer.GetSwapChain().GetHeight(),
                 .format      = hitagi::gfx::Format::R8G8B8A8_UNORM,
                 .clear_value = hitagi::math::vec4f{0.0f, 0.0f, 0.0f, 1.0f},
                 .usages      = hitagi::gfx::TextureUsageFlags::RenderTarget | hitagi::gfx::TextureUsageFlags::CopySrc,
            });
        renderer.RenderScene(scene, scene->curr_camera, render_target);
        render_target = renderer.GetRenderGraph().MoveFrom(render_target);
        renderer.RenderGui(render_target, true);
        renderer.ToSwapChain(render_target);

        engine.Tick();
    }

    return 0;
}