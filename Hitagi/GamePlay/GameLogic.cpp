#include "GameLogic.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int GameLogic::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GameLogic");
    m_Logger->info("Initialize...");

    return 0;
}

void GameLogic::Tick() {}

void GameLogic::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
