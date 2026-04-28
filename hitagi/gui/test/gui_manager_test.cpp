#include "test_macros.hpp"
#include <imgui.h>

import app;
import core;
import gfx;
import gui;
import test_utils;

using namespace hitagi;

class GuiManagerTest : public ::testing::Test {
protected:
    GuiManagerTest()
        : app(Application::CreateApp({
              .title    = ::testing::UnitTest::GetInstance()->current_test_info()->name(),
              .width    = 640,
              .height   = 480,
              .headless = true,
          })),
          gui_manager(std::make_unique<gui::GuiManager>(*app)) {
        app->ResizeWindow(640, 480);
        app->Tick();
    }

    std::unique_ptr<Application>     app;
    std::unique_ptr<gui::GuiManager> gui_manager;
};

TEST_F(GuiManagerTest, BuildsDrawDataFromImGuiFrame) {
    gui_manager->DrawGui([] {
        ImGui::SetNextWindowPos(ImVec2{20.0f, 20.0f});
        ImGui::SetNextWindowSize(ImVec2{200.0f, 120.0f});
        ImGui::Begin("DrawDataTest");
        ImGui::TextUnformatted("Hello Hitagi");
        ImGui::End();
    });

    gui_manager->Tick();

    const auto& draw_data = gui_manager->GetDrawData();
    EXPECT_FALSE(draw_data.font_atlas.Empty());
    EXPECT_FALSE(draw_data.Empty());

    bool has_font_draw = false;
    for (const auto& draw_list : draw_data.draw_lists) {
        for (const auto& command : draw_list.commands) {
            has_font_draw = has_font_draw || command.texture.type == gui::GuiTextureRef::Type::Font;
        }
    }
    EXPECT_TRUE(has_font_draw);
}

TEST_F(GuiManagerTest, MapsReadTextureToDrawCommand) {
    constexpr rg::TextureHandle texture{42};

    gui_manager->DrawGui([&] {
        ImGui::SetNextWindowPos(ImVec2{20.0f, 20.0f});
        ImGui::SetNextWindowSize(ImVec2{200.0f, 120.0f});
        ImGui::Begin("TextureRefTest");
        ImGui::Image(gui_manager->ReadTexture(texture), ImVec2{16.0f, 16.0f});
        ImGui::End();
    });

    gui_manager->Tick();

    bool found_texture = false;
    for (const auto& draw_list : gui_manager->GetDrawData().draw_lists) {
        for (const auto& command : draw_list.commands) {
            found_texture = found_texture ||
                            (command.texture.type == gui::GuiTextureRef::Type::RenderGraph &&
                             command.texture.texture == texture);
        }
    }

    EXPECT_TRUE(found_texture);
}
