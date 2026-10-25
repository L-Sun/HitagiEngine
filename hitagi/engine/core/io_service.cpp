module;

#include "interop/tracy_macros.hpp"

export module core:io_service;
import interop.spdlog;
import interop.tracy;

import std;

import utils;
import :buffer;

export namespace hitagi::core {

// Returns owning snapshots: callers may retain data while another thread reloads it.
class IOService {
public:
    IOService() : m_Logger(utils::try_create_logger("IOService")) {}
    auto SyncOpenAndReadBinary(const std::filesystem::path& path) -> Buffer;
    void SaveString(const std::filesystem::path& path, std::string_view text);
    void SaveBuffer(const std::filesystem::path& path, const Buffer& buffer);
    void SaveBuffer(const std::filesystem::path& path, std::span<const std::byte> buffer);

private:
    struct CachedFile {
        std::filesystem::file_time_type modified;
        Buffer                          data;
    };
    std::shared_ptr<spdlog::logger>                  m_Logger;
    std::mutex                                       m_Mutex;
    std::pmr::map<std::filesystem::path, CachedFile> m_Cache;
};

}  // namespace hitagi::core

namespace hitagi::core {

auto IOService::SyncOpenAndReadBinary(const std::filesystem::path& path) -> Buffer {
    const auto       key = std::filesystem::absolute(path).lexically_normal();
    std::scoped_lock lock(m_Mutex);
    if (!std::filesystem::exists(key)) {
        m_Logger->warn("File does not exist: {}", key.string());
        return {};
    }
    const auto modified = std::filesystem::last_write_time(key);
    if (const auto found = m_Cache.find(key); found != m_Cache.end() && found->second.modified == modified)
        return found->second.data;
    const auto size = std::filesystem::file_size(key);
    if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
        size > std::numeric_limits<std::size_t>::max()) throw std::length_error("File too large");
    std::ifstream stream(key, std::ios::binary);
    Buffer        buffer(static_cast<std::size_t>(size));
    if (!stream || (size && !stream.read(reinterpret_cast<char*>(buffer.GetData()), static_cast<std::streamsize>(size))))
        throw std::runtime_error("Cannot read file: " + key.string());
    const auto [entry, inserted] = m_Cache.insert_or_assign(key, CachedFile{modified, std::move(buffer)});
    return entry->second.data;
}

void IOService::SaveString(const std::filesystem::path& path, std::string_view text) {
    SaveBuffer(path, std::as_bytes(std::span(text.data(), text.size())));
}
void IOService::SaveBuffer(const std::filesystem::path& path, const Buffer& buffer) {
    SaveBuffer(path, buffer.Span<const std::byte>());
}
void IOService::SaveBuffer(const std::filesystem::path& path, std::span<const std::byte> buffer) {
    const auto       key = std::filesystem::absolute(path).lexically_normal();
    std::scoped_lock lock(m_Mutex);
    std::ofstream    stream(key, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    stream.close();
    m_Cache.erase(key);
    if (!stream) throw std::runtime_error("Cannot write file: " + key.string());
}

}  // namespace hitagi::core
