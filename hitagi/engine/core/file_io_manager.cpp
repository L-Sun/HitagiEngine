module;

#include "interop/tracy_macros.hpp"

export module core:file_io_manager;
import interop.spdlog;
import interop.tracy;

import std;

import utils;
import :buffer;

export namespace hitagi::core {

class FileIOManager {
public:
    FileIOManager() : m_Logger(utils::try_create_logger("FileIOManager")) {}

    auto SyncOpenAndReadBinary(const std::filesystem::path& file_path) -> const Buffer&;
    void SaveString(std::string_view str, const std::filesystem::path& path);
    void SaveBuffer(const Buffer& buffer, const std::filesystem::path& path);
    void SaveBuffer(std::span<const std::byte> buffer, const std::filesystem::path& path);

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    bool          IsFileChanged(const std::filesystem::path& file_path) const;
    const Buffer& CacheFile(const std::filesystem::path& file_path, Buffer buffer);

    using PathHash = std::size_t;

    TracyLockableN(std::mutex, m_CacheMutex, "FileIO Cache Mutex");
    std::pmr::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::pmr::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                             m_EmptyBuffer;
};

}  // namespace hitagi::core

namespace hitagi::core {

bool FileIOManager::IsFileChanged(const std::filesystem::path& file_path) const {
    PathHash hash = std::filesystem::hash_value(file_path);
    if (!m_FileStateCache.contains(hash)) return true;
    return m_FileStateCache.at(hash) < std::filesystem::last_write_time(file_path);
}

auto FileIOManager::SyncOpenAndReadBinary(const std::filesystem::path& file_path) -> const Buffer& {
    if (!std::filesystem::exists(file_path)) {
        m_Logger->warn("File does not exist. {}", file_path.string());
        return m_EmptyBuffer;
    }
    if (!IsFileChanged(file_path)) {
        m_Logger->trace("Use cache: {}", file_path.filename().string());
        return m_FileCache.at(std::filesystem::hash_value(file_path));
    }
    auto file_size = std::filesystem::file_size(file_path);
    if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
        file_size > std::numeric_limits<std::size_t>::max()) {
        m_Logger->error("File is too large to read. {}", file_path.string());
        return m_EmptyBuffer;
    }
    m_Logger->trace("Open file: {} ({} bytes)", file_path.string(), file_size);
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs) {
        m_Logger->error("Cannot open file. {}", file_path.string());
        return m_EmptyBuffer;
    }
    Buffer buffer(static_cast<std::size_t>(file_size));
    if (file_size != 0 && !ifs.read(reinterpret_cast<char*>(buffer.GetData()), static_cast<std::streamsize>(file_size))) {
        m_Logger->error("Cannot read complete file. {}", file_path.string());
        return m_EmptyBuffer;
    }

    // store cache
    return CacheFile(file_path, std::move(buffer));
}

void FileIOManager::SaveString(std::string_view str, const std::filesystem::path& path) {
    SaveBuffer(std::span{reinterpret_cast<const std::byte*>(str.data()), str.size()}, path);
}

void FileIOManager::SaveBuffer(const Buffer& buffer, const std::filesystem::path& path) {
    SaveBuffer(buffer.Span<const std::byte>(), path);
}

void FileIOManager::SaveBuffer(std::span<const std::byte> buffer, const std::filesystem::path& path) {
    auto fs = std::fstream(path, std::ios::binary | std::ios::out);
    fs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    fs.close();
    m_Logger->trace("Buffer has write to: {} ({} bytes)", path.string(), buffer.size());
}

const Buffer& FileIOManager::CacheFile(const std::filesystem::path& path, Buffer buffer) {
    std::scoped_lock lock{m_CacheMutex};

    PathHash hash          = std::filesystem::hash_value(path);
    m_FileStateCache[hash] = std::filesystem::last_write_time(path);
    m_FileCache[hash]      = std::move(buffer);

    return m_FileCache[hash];
}

}  // namespace hitagi::core
