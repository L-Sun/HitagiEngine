#include "MyGame.hpp"

#include "FileIOManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int MyGame::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MyGame");
    return 0;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
}
