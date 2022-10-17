#include <hitagi/asset/scene_parser.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::asset {
SceneParser::SceneParser(std::shared_ptr<spdlog::logger> logger) : m_Logger(logger ? std::move(logger) : spdlog::default_logger()) {}
}  // namespace hitagi::asset