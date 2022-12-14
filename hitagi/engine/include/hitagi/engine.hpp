#pragma once
#include <hitagi/core/core.hpp>
#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/hid/input_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>

#include <vector>
#include <set>
#include <concepts>

namespace hitagi {
class Engine : public RuntimeModule {
public:
    Engine(std::unique_ptr<Application> application);

    void Tick() final;

private:
    std::uint64_t m_FrameIndex = 0;
};

extern Engine* engine;

}  // namespace hitagi