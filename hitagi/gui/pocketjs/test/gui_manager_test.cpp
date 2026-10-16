#include "test_macros.hpp"
import interop.gtest;

import std;
import app;
import core;
import gui;
import hid;

using namespace hitagi;

class PocketGuiManagerTest : public ::testing::Test {
protected:
    core::FileIOManager          file_io;
    std::unique_ptr<Application> app = Application::CreateApp({
        .title    = "PocketJS test",
        .width    = 800,
        .height   = 800,
        .headless = true,
    });
    gui::GuiManager              manager{*app, file_io};

    void Load() {
        manager.LoadApp("build/pocketjs/ui/input-fixture.js", "build/pocketjs/ui/input-fixture.pak");
        manager.Tick();
    }
};

TEST_F(PocketGuiManagerTest, EmptyUntilExplicitLoad) {
    manager.Tick();
    EXPECT_TRUE(manager.GetDrawData().Empty());
    Load();
    const auto& data = manager.GetDrawData();
    ASSERT_FALSE(data.Empty());
    ASSERT_FALSE(data.textures.empty());
    ASSERT_TRUE(data.draw_lists.front().vertices.size() > 4u);
    // Background is vertex color, not a full-window bitmap.
    const auto& color = data.draw_lists.front().vertices.front().color;
    EXPECT_FLOAT_EQ(color[0], 15.0f / 255);
    EXPECT_FLOAT_EQ(color[1], 23.0f / 255);
    EXPECT_FLOAT_EQ(color[2], 42.0f / 255);
    EXPECT_FLOAT_EQ(color[3], 1.0f);
    for (const auto& texture : data.textures)
        EXPECT_EQ(texture->pixels.size(), std::size_t(texture->width) * texture->height * 4);
}

TEST_F(PocketGuiManagerTest, LetterboxesAndKeepsIdleTexture) {
    Load();
    const auto& data = manager.GetDrawData();
    ASSERT_EQ(data.draw_lists.size(), 1u);
    const auto& vertices = data.draw_lists.front().vertices;
    ASSERT_TRUE(vertices.size() > 4u);
    const auto width  = data.display_size.x;
    const auto height = data.display_size.y;
    EXPECT_FLOAT_EQ(vertices[0].position.x, 0.0f);
    EXPECT_FLOAT_EQ(vertices[0].position.y, (height - width * 540.0f / 960.0f) * 0.5f);
    EXPECT_FLOAT_EQ(vertices[2].position.x, width);
    const auto generation = data.textures.front()->id;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    EXPECT_EQ(data.textures.front()->id, generation);
}

TEST_F(PocketGuiManagerTest, FailedReloadPreservesAppAndSuccessfulReloadInvalidatesTexture) {
    Load();
    const auto generation = manager.GetDrawData().textures.front()->id;
    EXPECT_THROW(manager.LoadApp("missing-pocketjs-bundle.js", "missing-pocketjs-pack.pak"), std::runtime_error);
    manager.Tick();
    EXPECT_FALSE(manager.GetDrawData().Empty());
    EXPECT_EQ(manager.GetDrawData().textures.front()->id, generation);
    Load();
    EXPECT_GT(manager.GetDrawData().textures.front()->id, generation);
}

TEST_F(PocketGuiManagerTest, PointerUsesLetterboxCoordinates) {
    Load();
    auto counter_hash = [&] {
        const auto& list = manager.GetDrawData().draw_lists.front();
        const auto counter = manager.DisplayRect({365, 240, 535, 45});
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& vertex : list.vertices) {
            // Restrict the hash to counter glyphs; button focus/press styling
            // must not let this test pass when the action itself fails.
            if (vertex.position.x < counter.x || vertex.position.x >= counter.x + counter.z ||
                vertex.position.y < counter.y || vertex.position.y >= counter.y + counter.w) continue;
            for (const float value : {vertex.position.x, vertex.position.y, vertex.uv.x, vertex.uv.y})
                hash = (hash ^ std::bit_cast<std::uint32_t>(value)) * 1099511628211ull;
        }
        return hash;
    };
    const auto  before = counter_hash();
    const auto& data   = manager.GetDrawData();
    const auto  origin = data.draw_lists.front().vertices.front().position;
    const auto  scale  = data.display_size.x / 960.0f;
    auto&       input  = app->GetInputManager();
    input.UpdatePointerState(origin.x + 70.0f * scale, origin.y + 240.0f * scale);
    input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    input.Tick();
    input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    EXPECT_NE(counter_hash(), before);
}

TEST_F(PocketGuiManagerTest, NativeMailboxAndFrameLocalTextures) {
    EXPECT_THROW(manager.PostEvent("{}"), std::logic_error);
    Load();
    EXPECT_TRUE(manager.DrainEffects().empty());
    EXPECT_NO_THROW(manager.PostEvent(R"({"type":"unused"})"));
    manager.DrawTexture({}, {200, 80, 520, 320});
    manager.Tick();
    ASSERT_EQ(manager.GetDrawData().draw_lists.size(), 2u);
    const auto& command = manager.GetDrawData().draw_lists.back().commands.front();
    EXPECT_EQ(command.texture.type, gui::GuiTextureRef::Type::RenderGraph);
    manager.Tick();
    EXPECT_EQ(manager.GetDrawData().draw_lists.size(), 1u);
}

TEST_F(PocketGuiManagerTest, TextureFormatsReuseAndFrameOwnership) {
    std::filesystem::create_directories("temp/pocketjs");
    const auto source = std::filesystem::path("temp/pocketjs/gpu-resource-test.js");
    {
        std::ofstream file(source);
        // Use real HostOps, including freeing/reusing a native texture slot.
        file << R"JS(
const nodes = [];
const textures = [];
const palette = new Uint8Array(1028);
palette.set([0, 0, 255, 255]);
for (const [format, bytes] of [
    [0, [31,0,31,0,31,0,31,0]],
    [2, [0,255,0,255,0,255,0,255]],
    [3, [0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255]],
    [5, palette]
]) {
    const node = ui.createNode(2);
    ui.setProp(node, 1, 32); ui.setProp(node, 2, 32);
    ui.insertBefore(1, node, 0);
    const texture = ui.uploadTexture(new Uint8Array(bytes), 2, 2, format);
    ui.setImage(node, texture);
    nodes.push(node); textures.push(texture);
}
let changed = false;
globalThis.frame = (buttons) => {
    if (buttons && !changed) {
        changed = true;
        ui.freeTexture(textures[0]);
        const texture = ui.uploadTexture(new Uint8Array(16).fill(255), 2, 2, 3);
        ui.setImage(nodes[0], texture);
    }
};
)JS";
    }
    manager.LoadApp(source, "build/pocketjs/ui/input-fixture.pak");
    manager.Tick();
    const auto before = manager.GetDrawData(); // Owns resources across runtime mutation.
    const auto image = [&](const gui::GuiDrawData& data, unsigned index) {
        const auto id = data.draw_lists.front().commands.at(index).texture.resource;
        const auto it = std::ranges::find_if(data.textures, [&](const auto& p) { return p->id == id; });
        return it == data.textures.end() ? std::shared_ptr<const gui::GuiTextureData>{} : *it;
    };
    ASSERT_EQ(before.draw_lists.front().commands.size(), 4u);
    const auto red = image(before, 0), rgba4444 = image(before, 1), green = image(before, 2), blue = image(before, 3);
    ASSERT_TRUE(red && rgba4444 && green && blue);
    EXPECT_EQ(red->pixels[0], std::byte{255});
    EXPECT_EQ(red->pixels[1], std::byte{0});
    EXPECT_EQ(rgba4444->pixels[0], std::byte{0}); // 0xff00 = opaque blue in ABGR4444.
    EXPECT_EQ(rgba4444->pixels[1], std::byte{0});
    EXPECT_EQ(rgba4444->pixels[2], std::byte{255});
    EXPECT_EQ(green->pixels[1], std::byte{255});
    EXPECT_EQ(blue->pixels[2], std::byte{255});
    auto& input = app->GetInputManager();
    input.UpdateKeyState(hid::VirtualKeyCode::KEY_ENTER, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    const auto after = manager.GetDrawData();
    const auto white = image(after, 0);
    ASSERT_TRUE(white);
    EXPECT_NE(white->id, red->id);
    EXPECT_EQ(white->pixels[1], std::byte{255});
    EXPECT_EQ(image(after, 2), green); // Unchanged textures share the exact snapshot.
    EXPECT_EQ(red->pixels[1], std::byte{0}); // Previous frame remains valid.
    EXPECT_FALSE(std::ranges::any_of(after.textures, [&](const auto& p) { return p->id == red->id; }));
}

TEST_F(PocketGuiManagerTest, DesktopDensityResizeAndWidePointer) {
    app->ResizeWindow(1800, 1200);
    manager.LoadApp("build/pocketjs/ui/hitagi-ui.js", "build/pocketjs/ui/hitagi-ui.pak", {.desktop = true, .raster_density = 2});
    manager.Tick();
    const auto dpi            = std::max(app->GetDpiRatio(), 1.0f);
    const auto logical_width  = static_cast<unsigned>(std::round(app->GetWindowWidth() / dpi));
    const auto logical_height = static_cast<unsigned>(std::round(app->GetWindowHeight() / dpi));
    EXPECT_NEAR(manager.GetDrawData().draw_lists.front().vertices[2].position.x, logical_width * dpi, 0.01f);
    EXPECT_NEAR(manager.GetDrawData().draw_lists.front().vertices[2].position.y, logical_height * dpi, 0.01f);
    EXPECT_FLOAT_EQ(manager.GetDrawData().draw_lists.front().vertices.front().position.y, 0);
    const auto initial_effects = manager.DrainEffects();
    EXPECT_TRUE(std::ranges::any_of(initial_effects, [](const auto& line) { return line.find("\"action\":\"layout\"") != std::string::npos; }));
    const auto generation = manager.GetDrawData().textures.front()->id;
    app->ResizeWindow(2100, 1300);
    manager.Tick();
    EXPECT_EQ(manager.GetDrawData().textures.front()->id, generation); // Resize reuses the atlas.
    EXPECT_NEAR(manager.GetDrawData().draw_lists.front().vertices[2].position.x, std::round(app->GetWindowWidth() / dpi) * dpi, 0.01f);
    const auto resized_effects = manager.DrainEffects();
    EXPECT_TRUE(std::ranges::any_of(resized_effects, [](const auto& line) { return line.find("\"action\":\"layout\"") != std::string::npos; }));
    // Close-preview is at the far right; its logical X is greater than 1023.
    manager.PostEvent(R"({"type":"state","value":{"kind":"editor","scene":"Test","mode":"Edit","dirty":false,"canUndo":false,"canRedo":false,"selected":"","nodes":[],"assets":[],"transform":null,"status":"Ready","assetError":"","preview":"Texture"}})");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    manager.DrainEffects();
    const float width  = std::round(app->GetWindowWidth() / dpi);
    const float height = std::round(app->GetWindowHeight() / dpi);
    const auto  point  = manager.DisplayRect({width - 40, height - 218, 1, 1});
    auto&       input  = app->GetInputManager();
    input.UpdatePointerState(point.x, point.y);
    input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    input.Tick();
    input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    const auto effects = manager.DrainEffects();
    EXPECT_TRUE(std::ranges::any_of(effects, [](const auto& line) { return line.find("\"action\":\"closePreview\"") != std::string::npos; }));
    input.Tick();
    input.UpdateKeyState(hid::VirtualKeyCode::MOUSE_L_BUTTON, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    input.ResetInputState();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    manager.Tick();
    const auto cancelled = manager.DrainEffects();
    EXPECT_FALSE(std::ranges::any_of(cancelled, [](const auto& line) { return line.find("\"action\":\"closePreview\"") != std::string::npos; }));
}

TEST_F(PocketGuiManagerTest, ResizePreparesLayoutBeforeNativeTexturesWithoutStretching) {
    manager.LoadApp("build/pocketjs/ui/hitagi-ui.js", "build/pocketjs/ui/hitagi-ui.pak", {.desktop = true, .raster_density = 2});
    manager.Tick();
    manager.DrainEffects();
    const auto dpi = std::max(app->GetDpiRatio(), 1.0f);
    for (const auto size : {std::pair{1400u, 600u}, std::pair{900u, 1100u}, std::pair{1800u, 800u}}) {
        app->ResizeWindow(size.first, size.second);
        // Even before the UI resize tick, desktop geometry keeps one uniform
        // scale instead of stretching old logical dimensions to a new window.
        const auto square = manager.DisplayRect({20, 30, 100, 100});
        EXPECT_FLOAT_EQ(square.z, 100 * dpi);
        EXPECT_FLOAT_EQ(square.w, square.z);
        manager.PrepareFrame();
        const auto effects = manager.DrainEffects();
        EXPECT_TRUE(std::ranges::any_of(effects, [](const auto& line) { return line.find("\"action\":\"layout\"") != std::string::npos; }));
        manager.PrepareFrame();
        EXPECT_TRUE(manager.DrainEffects().empty());
        manager.DrawTexture({}, {20, 30, 100, 100});
        manager.Tick();
        EXPECT_TRUE(manager.DrainEffects().empty());  // Tick must not advance layout a second time.
        const auto& data = manager.GetDrawData();
        ASSERT_EQ(data.draw_lists.size(), 2u);
        const auto& quad = data.draw_lists.back().vertices;
        EXPECT_FLOAT_EQ(quad[2].position.x - quad[0].position.x, square.z);
        EXPECT_FLOAT_EQ(quad[2].position.y - quad[0].position.y, square.w);
        EXPECT_NEAR(data.draw_lists.front().vertices[2].position.x, std::round(size.first / dpi) * dpi, 0.01f);
        EXPECT_NEAR(data.draw_lists.front().vertices[2].position.y, std::round(size.second / dpi) * dpi, 0.01f);
    }
}
