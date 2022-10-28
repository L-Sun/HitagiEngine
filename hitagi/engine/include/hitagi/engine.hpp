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
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "Engine"; }

private:
    std::uint64_t m_FrameIndex = 0;
};

extern Engine* engine;

}  // namespace hitagi