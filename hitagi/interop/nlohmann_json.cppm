module;
#include <nlohmann/json.hpp>

export module interop.nlohmann_json;

export namespace nlohmann {
using ::nlohmann::json;
using ::nlohmann::ordered_json;
}  // namespace nlohmann
