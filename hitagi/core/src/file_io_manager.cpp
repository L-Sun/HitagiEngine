#include <hitagi/core/file_io_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fstream>
#include <mutex>

namespace hitagi {
core::FileIOManager* file_io_manager = nullptr;
}

namespace hitagi::core {

bool FileIOManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("FileIOManager");
    m_Logger->info("Initialize...");
    return true;
}
void FileIOManager::Finalize() {
    m_FileStateCache.clear();
    m_FileCache.clear();
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

bool FileIOManager::IsFileChanged(const std::filesystem::path& file_path) const {
    PathHash hash = std::filesystem::hash_value(file_path);
    if (!m_FileStateCache.contains(hash)) return true;
    return m_FileStateCache.at(hash) < std::filesystem::last_write_time(file_path);
}

const Buffer& FileIOManager::SyncOpenAndReadBinary(const std::filesystem::path& file_path) {
    if (!std::filesystem::exists(file_path)) {
        m_Logger->warn("File dose not exist. {}", file_path.string());
        return m_EmptyBuffer;
    }
    if (!IsFileChanged(file_path)) {
        m_Logger->debug("Use cahce: {}", file_path.filename().string());
        return m_FileCache.at(std::filesystem::hash_value(file_path));
    }
    auto file_size = std::filesystem::file_size(file_path);
    m_Logger->info("Open file: {} ({} bytes)", file_path.string(), file_size);
    Buffer        buffer(file_size);
    std::ifstream ifs(file_path, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buffer.GetData()), buffer.GetDataSize());
    ifs.close();

    // store cache
    return CacheFile(file_path, std::move(buffer));
}

void FileIOManager::SaveBuffer(const Buffer& buffer, const std::filesystem::path& path) {
    auto fs = std::fstream(path, std::ios::binary | std::ios::out);
    fs.write(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());
    fs.close();
    m_Logger->info("Buffer has write to: {} ({} bytes)", path.string(), buffer.GetDataSize());
}

const Buffer& FileIOManager::CacheFile(const std::filesystem::path& path, Buffer buffer) {
    std::lock_guard lock{m_CacheMutex};

    PathHash hash          = std::filesystem::hash_value(path);
    m_FileStateCache[hash] = std::filesystem::last_write_time(path);
    m_FileCache[hash]      = std::move(buffer);

    return m_FileCache[hash];
}

}  // namespace hitagi::core