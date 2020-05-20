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
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
void FileIOManager::Tick() {}

size_t FileIOManager::GetFileSize(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath);
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

Buffer FileIOManager::SyncOpenAndReadBinary(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath);
        return Buffer();
    }

    m_Logger->info("Open file: {}", filePath);
    Buffer        buf(std::filesystem::file_size(filePath));
    std::ifstream ifs(filePath, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buf.GetData()), buf.GetDataSize());
    ifs.close();
    return buf;
}

std::string FileIOManager::SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath);
        return std::string();
    }
    m_Logger->info("Open file: {}", filePath);
    std::ifstream ifs(filePath);
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

}  // namespace Hitagi::Core