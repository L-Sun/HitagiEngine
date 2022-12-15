#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/gui/gui_manager.hpp>

using namespace hitagi;

class ImGuiDemo : public hitagi::RuntimeModule {
public:
    ImGuiDemo(gui::GuiManager& gui_manager) : hitagi::RuntimeModule("ImGuiDemo"), gui_manager(gui_manager) {}
    void Tick() final;

    gui::GuiManager& gui_manager;
};