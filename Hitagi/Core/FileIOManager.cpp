#include "FileIOManager.hpp"

#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/ostream.h>

namespace hitagi {
std::unique_ptr<core::FileIOManager> g_FileIoManager = std::make_unique<core::FileIOManager>();
}

namespace hitagi::core {

int FileIOManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("FileIOManager");
    m_Logger->info("Initialize...");
    return 0;
}
void FileIOManager::Finalize() {
    m_FileStateCache.clear();
    m_FileCache.clear();
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
void FileIOManager::Tick() {}

bool FileIOManager::IsFileChanged(const std::filesystem::path& file_path) const {
    PathHash hash = std::filesystem::hash_value(file_path);
    if (m_FileStateCache.count(hash) == 0) return true;
    return m_FileStateCache.at(hash) < std::filesystem::last_write_time(file_path);
}

const Buffer& FileIOManager::SyncOpenAndReadBinary(const std::filesystem::path& file_path) {
    if (!std::filesystem::exists(file_path)) {
        m_Logger->warn("File dose not exist. {}", file_path);
        return m_EmptyBuffer;
    }
    if (!IsFileChanged(file_path)) {
        m_Logger->debug("Use cahce: {}", file_path.filename());
        return m_FileCache.at(std::filesystem::hash_value(file_path));
    }
    auto file_size = std::filesystem::file_size(file_path);
    m_Logger->info("Open file: {} ({} bytes)", file_path, file_size);
    Buffer        buffer(file_size);
    std::ifstream ifs(file_path, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buffer.GetData()), buffer.GetDataSize());
    ifs.close();

    // store cache
    PathHash hash          = std::filesystem::hash_value(file_path);
    m_FileStateCache[hash] = std::filesystem::last_write_time(file_path);
    m_FileCache[hash]      = std::move(buffer);

    return m_FileCache[hash];
}

void FileIOManager::SaveBuffer(const Buffer& buffer, const std::filesystem::path& path) {
    auto fs = std::fstream(path, std::ios::binary | std::ios::out);
    fs.write(reinterpret_cast<const char*>(buffer.GetData()), buffer.GetDataSize());
    fs.close();
    m_Logger->info("Buffer has write to: {} ({} bytes)", path, buffer.GetDataSize());
}

}  // namespace hitagi::core