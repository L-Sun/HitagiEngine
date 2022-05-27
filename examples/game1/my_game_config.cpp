#include "my_game.hpp"
#include <hitagi/application.hpp>

namespace hitagi {
std::unique_ptr<GamePlay> g_GamePlay = std::make_unique<MyGame>();
}  // namespace hitagi