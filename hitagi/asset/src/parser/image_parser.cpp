#include <hitagi/asset/parser/image_parser.hpp>

#include <spdlog/spdlog.h>

namespace hitagi::asset {
ImageParser::ImageParser(std::shared_ptr<spdlog::logger> logger) : m_Logger(logger ? std::move(logger) : spdlog::default_logger()) {}
}  // namespace hitagi::asset