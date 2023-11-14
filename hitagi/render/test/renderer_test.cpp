#include <hitagi/core/memory_manager.hpp>
#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/asset/mesh_factory.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/utils/test.hpp>
#include <tracy/Tracy.hpp>
#include "imgui.h"

using namespace testing;
using namespace hitagi;
using namespace hitagi::render;

constexpr std::array supported_device_types = {
#ifdef _WIN32
    gfx::Device::Type::DX12,
#endif
    gfx::Device::Type::Vulkan,
};

class RendererTest : public TestWithParam<gfx::Device::Type> {
protected:
    RendererTest()
        : test_name(UnitTest::GetInstance()->current_test_info()->name()),
          app(Application::CreateApp(AppConfig{
              .gfx_backend = std::pmr::string(magic_enum::enum_name(GetParam())),
          })),
          device(gfx::Device::Create(GetParam())) {}

    std::pmr::string             test_name;
    std::unique_ptr<Application> app;
    std::unique_ptr<gfx::Device> device;
};
INSTANTIATE_TEST_SUITE_P(
    RendererTest,
    RendererTest,
    ValuesIn(supported_device_types),
    [](const TestParamInfo<gfx::Device::Type>& info) -> std::string {
        return std::string{magic_enum::enum_name(info.param)};
    });

TEST_P(RendererTest, ForwardRenderer) {
    auto            gui_manager = std::make_unique<gui::GuiManager>(*app);
    ForwardRenderer renderer(*device, *app, gui_manager.get(), test_name);

    asset::AssetManager asset_manager("./assets");

    auto scene = asset_manager.ImportScene("assets/scenes/untitled.fbx");

    std::size_t frame_index = 0;
    while (!app->IsQuit()) {
        auto texture = renderer.GetRenderGraph().Create(gfx::TextureDesc{
            .name        = std::pmr::string{fmt::format("RenderTarget-{}", frame_index)},
            .width       = renderer.GetSwapChain().GetWidth(),
            .height      = renderer.GetSwapChain().GetHeight(),
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::vec4f(0.0, 0.0, 0.0, 1.0),
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
        });

        gui_manager->DrawGui([=]() {
            ImGui::DragFloat3("Light Position", scene->light_nodes.front()->transform.local_translation, 0.1f);
            ImGui::DragFloat3("Camera Position", scene->curr_camera->transform.local_translation, 0.1f);
            ImGui::DragFloat3("cube Position", scene->instance_nodes.front()->transform.local_translation, 0.1f);
        });
        gui_manager->Tick();

        scene->curr_camera->GetObjectRef()->parameters.aspect = static_cast<float>(renderer.GetSwapChain().GetWidth()) / static_cast<float>(renderer.GetSwapChain().GetHeight());

        scene->Update();
        renderer.RenderScene(scene, scene->curr_camera, texture);
        texture = renderer.GetRenderGraph().MoveFrom(texture);
        renderer.RenderGui(texture, false);
        renderer.ToSwapChain(texture);
        renderer.Tick();

        app->Tick();

        FrameMark;

        if (frame_index++ == 3) {
            break;
        }
    }
    asset::Texture::DestroyDefaultTexture();
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);

    auto g_memory_manager = std::make_unique<core::MemoryManager>();
    auto g_file_manager   = std::make_unique<core::FileIOManager>();

    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}