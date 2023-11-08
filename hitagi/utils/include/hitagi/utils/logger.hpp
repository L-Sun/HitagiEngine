#pragma once

#include <memory>
#include <string_view>

namespace spdlog {
class logger;
}

namespace hitagi::utils {

auto try_create_logger(std::string_view name) -> std::shared_ptr<spdlog::logger>;

}