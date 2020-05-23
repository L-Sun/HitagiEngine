#include "FileIOManager.hpp"

#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/ostream.h>

namespace Hitagi {
std::unique_ptr<Core::FileIOManager> g_FileIOManager = std::make_unique<Core::FileIOManager>();
}

namespace Hitagi::Core {

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

bool FileIOManager::IsFileChanged(const std::filesystem::path& filePath) const {
    PathHash hash = std::filesystem::hash_value(filePath);
    if (m_FileStateCache.count(hash) == 0) return true;
    return m_FileStateCache.at(hash) < std::filesystem::last_write_time(filePath);
}

const Buffer& FileIOManager::SyncOpenAndReadBinary(const std::filesystem::path& filePath) {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath);
        return m_EmptyBuffer;
    }
    if (!IsFileChanged(filePath)) {
        m_Logger->debug("Use cahce: {}", filePath.filename());
        return m_FileCache.at(std::filesystem::hash_value(filePath));
    }

    m_Logger->info("Open file: {}", filePath);
    Buffer        buffer(std::filesystem::file_size(filePath));
    std::ifstream ifs(filePath, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buffer.GetData()), buffer.GetDataSize());
    ifs.close();

    // store cache
    PathHash hash          = std::filesystem::hash_value(filePath);
    m_FileStateCache[hash] = std::filesystem::last_write_time(filePath);
    m_FileCache[hash]      = std::move(buffer);

    return m_FileCache[hash];
}

}  // namespace Hitagi::Core