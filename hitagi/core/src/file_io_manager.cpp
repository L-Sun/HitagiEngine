#include <hitagi/core/file_io_manager.hpp>

#include <spdlog/logger.h>
#include <fstream>
#include <mutex>

namespace hitagi::core {

bool FileIOManager::IsFileChanged(const std::filesystem::path& file_path) const {
    PathHash hash = std::filesystem::hash_value(file_path);
    if (!m_FileStateCache.contains(hash)) return true;
    return m_FileStateCache.at(hash) < std::filesystem::last_write_time(file_path);
}

auto FileIOManager::SyncOpenAndReadBinary(const std::filesystem::path& file_path) -> const Buffer& {
    if (!std::filesystem::exists(file_path)) {
        m_Logger->warn("File dose not exist. {}", file_path.string());
        return m_EmptyBuffer;
    }
    if (!IsFileChanged(file_path)) {
        m_Logger->trace("Use cache: {}", file_path.filename().string());
        return m_FileCache.at(std::filesystem::hash_value(file_path));
    }
    auto file_size = std::filesystem::file_size(file_path);
    m_Logger->trace("Open file: {} ({} bytes)", file_path.string(), file_size);
    Buffer        buffer(file_size);
    std::ifstream ifs(file_path, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buffer.GetData()), buffer.GetDataSize());
    ifs.close();

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
    std::lock_guard lock{m_CacheMutex};

    PathHash hash          = std::filesystem::hash_value(path);
    m_FileStateCache[hash] = std::filesystem::last_write_time(path);
    m_FileCache[hash]      = std::move(buffer);

    return m_FileCache[hash];
}

}  // namespace hitagi::core