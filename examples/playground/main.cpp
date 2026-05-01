#include <spdlog/spdlog.h>
#include <imgui.h>
#include <combaseapi.h>
#include <filesystem>
#include <shlobj.h>

import engine;
import asset;

using namespace hitagi;

static auto GetLatestWinPixGpuCapturerPath_Cpp17() -> std::filesystem::path {
    LPWSTR programFilesPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &programFilesPath)) ||
        programFilesPath == nullptr) {
        return {};
    }

    std::filesystem::path pixInstallationPath = programFilesPath;
    CoTaskMemFree(programFilesPath);

    pixInstallationPath /= "Microsoft PIX";
    if (!std::filesystem::exists(pixInstallationPath)) {
        return {};
    }

    std::wstring newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath)) {
        if (directory_entry.is_directory()) {
            const auto version = directory_entry.path().filename().wstring();
            if (newestVersionFound.empty() || newestVersionFound < version) {
                newestVersionFound = version;
            }
        }
    }

    if (newestVersionFound.empty()) {
        return {};
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

auto main(int argc, char** argv) -> int {
    if (GetModuleHandleW(L"WinPixGpuCapturer.dll") == nullptr) {
        const auto pixCapturerPath = GetLatestWinPixGpuCapturerPath_Cpp17();
        if (!pixCapturerPath.empty()) {
            LoadLibraryW(pixCapturerPath.c_str());
        }
    }

    spdlog::set_level(spdlog::level::trace);

    hitagi::Engine engine;

    auto scene = std::make_shared<asset::Scene>("playground");

    auto  camera           = std::make_shared<asset::Camera>(asset::Camera::Parameters{});
    auto  camera_entity    = scene->CreateCameraEntity(camera, {}, {}, "");
    auto& camera_transform = camera_entity.Get<asset::Transform>();

    auto light     = scene->CreateLightEntity(std::make_shared<asset::Light>(asset::Light::Parameters{}), {}, {}, "");
    auto cube_mesh = asset::MeshFactory::Cube();
    auto cube      = scene->CreateMeshEntity(cube_mesh, {}, {}, "");

    for (auto& sub_mesh : cube_mesh->sub_meshes) {
        sub_mesh.material_instance = asset::AssetManager::Get()->GetMaterial("Phong")->CreateInstance();
    }

    while (!engine.App().IsQuit()) {
        engine.GuiManager().DrawGui([&]() {
            static bool open = true;
            if (ImGui::Begin("Cube info", &open)) {
                auto& cube_transform = cube.Get<asset::Transform>();

                ImGui::DragFloat3("Cube position", cube_transform.position, 0.01);
                ImGui::DragFloat3("Cube scaling", cube_transform.scaling, 0.01);

                ImGui::Separator();

                ImGui::DragFloat3("Camera position", camera_transform.position, 0.01);
                ImGui::DragFloat3("Camera eye", camera->parameters.eye, 0.01);
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
                .clear_value = math::Color::Black(),
                .usages      = hitagi::gfx::TextureUsageFlags::RenderTarget | hitagi::gfx::TextureUsageFlags::CopySrc,
            });

        renderer.RenderScene(scene, *camera, camera_transform.world_matrix, render_target);
        render_target = renderer.GetRenderGraph().MoveFrom(render_target);
        renderer.RenderGui(render_target, engine.GuiManager().GetDrawData(), true);
        renderer.ToSwapChain(render_target);

        engine.Tick();
    }

    return 0;
}
