export module ecs:types;
import std;

export namespace hitagi::ecs {
using entity_id_t    = std::uint64_t;
using archetype_id_t = std::uint64_t;

class Archetype;
class World;
class Schedule;
class Entity;
class EntityManager;
}  // namespace hitagi::ecs