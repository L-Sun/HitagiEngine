#include "GameLogic.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int GameLogic::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GameLogic");
    m_Logger->info("Initialize...");
    return 0;
}

void GameLogic::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void GameLogic::Tick() {}

void GameLogic::OnUpKeyDown() {}

void GameLogic::OnUpKeyUp() {}

void GameLogic::OnUpKey() {}

void GameLogic::OnDownKeyDown() {}

void GameLogic::OnDownKeyUp() {}

void GameLogic::OnDownKey() {}

void GameLogic::OnLeftKeyDown() {}

void GameLogic::OnLeftKeyUp() {}

void GameLogic::OnLeftKey() {}

void GameLogic::OnRightKeyDown() {}

void GameLogic::OnRightKeyUp() {}

void GameLogic::OnRightKey() {}

void GameLogic::OnRKeyDown() {}

void GameLogic::OnRKeyUp() {}

void GameLogic::OnRKey() {}

void GameLogic::OnDKeyDown() {}

void GameLogic::OnDKeyUp() {}

void GameLogic::OnDKey() {}
void GameLogic::OnCKeyDown() {}

void GameLogic::OnCKeyUp() {}

void GameLogic::OnCKey() {}