#pragma once
#include <exception>
#include <string_view>

namespace hitagi::utils {
class NoImplemented : public std::exception {
public:
    explicit NoImplemented(const char* message = "No Implemented!") : msg(message) {}
    NoImplemented(std::string_view message = "No Implemented!") : msg(message.data()) {}
    NoImplemented(NoImplemented const&) noexcept = default;

    NoImplemented& operator=(NoImplemented const&) noexcept = default;
    ~NoImplemented() override                               = default;

    const char* what() const noexcept override { return msg; }

private:
    const char* msg;
};
}  // namespace hitagi::utils