#pragma once
#include <cstdint>

namespace hitagi::ecs {

using entity_id_t    = std::uint64_t;
using archetype_id_t = std::uint64_t;

class Archetype;
class World;
class Schedule;
class Entity;

}  // namespace hitagi::ecs