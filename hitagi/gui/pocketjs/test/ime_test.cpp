#include "test_macros.hpp"
import interop.gtest;
#ifdef _WIN32
import interop.win32;
#endif
import std;
import app;
import core;
import gui;
import hid;

using namespace hitagi;

class PocketImeTest : public ::testing::Test {
protected:
    core::FileIOManager files;
    std::unique_ptr<Application> app = Application::CreateApp({.title = "IME integration", .width = 800, .height = 600, .headless = true});
    gui::GuiManager manager{*app, files};

    void SetUp() override {
        manager.LoadApp("build/pocketjs/ui/ime-fixture.js", "build/pocketjs/ui/ime-fixture.pak", {.desktop = true});
        Frame();
    }
    void Frame() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        manager.Tick();
        app->GetInputManager().Tick();
    }
    void Click(float x, float y) {
        auto& input = app->GetInputManager();
        const auto point = manager.DisplayRect({x, y, 0, 0});
        input.UpdatePointerState(point.x, point.y);
        input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);
        Frame();
        input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false);
        Frame();
    }
    void Send(hid::TextInputEvent event) { app->GetInputManager().PushTextInput(std::move(event)); }
    auto GeometryHash() const -> std::uint64_t {
        std::uint64_t hash = 0;
        for (const auto& list : manager.GetDrawData().draw_lists)
            for (const auto& vertex : list.vertices)
                for (float value : {vertex.position.x, vertex.position.y, vertex.uv.x, vertex.uv.y})
                    hash = (hash * 1099511628211ull) ^ std::bit_cast<std::uint32_t>(value);
        return hash;
    }
};

TEST_F(PocketImeTest, CompositionRendersAndCommitsExactlyOnce) {
    Click(40, 30);
    ASSERT_TRUE(manager.WantsTextInput());
    ASSERT_NE(app->GetInputManager().GetTextInputSession(), 0);
    const auto before = GeometryHash();
    Send({.kind = hid::TextInputEvent::Kind::Start});
    Send({.kind = hid::TextInputEvent::Kind::Update, .text = u"nihao", .caret = 3, .attributes = {0, 0, 1, 1, 1}, .clauses = {0, 2, 5}});
    Frame();
    EXPECT_NE(before, GeometryHash());
    EXPECT_TRUE(manager.DrainEffects().empty());
    Send({.kind = hid::TextInputEvent::Kind::Commit, .text = u"你好\"\\"});
    Send({.kind = hid::TextInputEvent::Kind::End});
    Frame();
    EXPECT_TRUE(manager.DrainEffects().empty());
    Send({.kind = hid::TextInputEvent::Kind::Key, .key = 13});
    Frame();
    const auto effects = manager.DrainEffects();
    ASSERT_EQ(effects.size(), 1);
    EXPECT_EQ(effects[0], R"({"type":"text.test","value":"你好\"\\","id":"first"})");
    Frame();
    EXPECT_FALSE(manager.WantsTextInput());
    EXPECT_TRUE(manager.DrainEffects().empty());
}

TEST_F(PocketImeTest, CancelAndFieldSwitchDiscardPreeditAndStaleEvents) {
    Click(40, 30);
    const auto session = app->GetInputManager().GetTextInputSession();
    Send({.kind = hid::TextInputEvent::Kind::Start});
    Send({.kind = hid::TextInputEvent::Kind::Update, .text = u"临时", .caret = 2});
    Frame();
    Click(40, 70);
    ASSERT_NE(app->GetInputManager().GetTextInputSession(), session);
    EXPECT_TRUE(manager.DrainEffects().empty());
    manager.PostEvent(std::format(R"({{"type":"state","value":{{"kind":"text","session":{},"event":"commit","text":"late"}}}})", session));
    Send({.kind = hid::TextInputEvent::Kind::Commit, .text = u"新名字"});
    Send({.kind = hid::TextInputEvent::Kind::Key, .key = 13});
    Frame();
    const auto effects = manager.DrainEffects();
    ASSERT_EQ(effects.size(), 1);
    EXPECT_EQ(effects[0], R"({"type":"text.test","value":"新名字","id":"second"})");
}

TEST_F(PocketImeTest, HeldAcceptCannotReactivateFieldAfterEditing) {
    Click(40, 30);
    auto& input = app->GetInputManager();
    input.UpdateKeyState(hid::VirtualKeyCode::KEY_ENTER, true);
    Send({.kind = hid::TextInputEvent::Kind::Key, .key = 13});
    Frame();
    Frame();
    Frame();
    EXPECT_FALSE(manager.WantsTextInput());
    EXPECT_EQ(input.GetTextInputSession(), 0);
    input.UpdateKeyState(hid::VirtualKeyCode::KEY_ENTER, false);
    Frame();
    input.UpdateKeyState(hid::VirtualKeyCode::KEY_ENTER, true);
    Frame();
    EXPECT_TRUE(manager.WantsTextInput());
}

#ifdef _WIN32
TEST_F(PocketImeTest, Win32SurrogatesCompositionKeysAndFocusLoss) {
    Click(40, 30);
    ASSERT_TRUE(manager.WantsTextInput());
    const auto window = static_cast<HWND>(app->GetWindow().ptr);
    SendMessageW(window, WM_IME_STARTCOMPOSITION, 0, 0);
    SendMessageW(window, WM_KEYDOWN, 13, 1);
    SendMessageW(window, WM_IME_ENDCOMPOSITION, 0, 0);
    SendMessageW(window, WM_KEYDOWN, 13, 1); // Auto-repeat of the consumed key.
    Frame();
    EXPECT_TRUE(manager.WantsTextInput());
    EXPECT_TRUE(manager.DrainEffects().empty());
    SendMessageW(window, WM_KEYUP, 13, 1);
    SendMessageW(window, WM_CHAR, 0xd83d, 1);
    SendMessageW(window, WM_CHAR, 0xde00, 1);
    SendMessageW(window, WM_IME_CHAR, L'重', 1); // Already consumed via GCS_RESULTSTR.
    SendMessageW(window, WM_KILLFOCUS, 0, 0);
    Frame();
    const auto effects = manager.DrainEffects();
    ASSERT_EQ(effects.size(), 1);
    EXPECT_EQ(effects[0], R"({"type":"text.test","value":"😀","id":"first"})");
    EXPECT_EQ(app->GetInputManager().GetTextInputSession(), 0);
}
#endif
