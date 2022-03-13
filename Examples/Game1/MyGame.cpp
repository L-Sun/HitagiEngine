#include "MyGame.hpp"
#include "FileIOManager.hpp"
#include "ThreadManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <queue>

using namespace Hitagi;

int MyGame::Initialize() {
    int ret  = 0;
    m_Logger = spdlog::stdout_color_mt("MyGame");
    m_Logger->info("Initialize...");

    if ((ret = m_Editor.Initialize()) != 0) return ret;

    return ret;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    m_Clock.Tick();
    m_Editor.Tick();
}
