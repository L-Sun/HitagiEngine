#include <iostream>
#include <fstream>
#include "FileIOManager.hpp"

namespace Hitagi::Core {

size_t FileIOManager::GetFileSize(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath.u8string());
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

Buffer FileIOManager::SyncOpenAndReadBinary(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath.u8string());
        return Buffer();
    }

    m_Logger->info("Open file: {}", filePath.u8string());
    Buffer        buf(std::filesystem::file_size(filePath));
    std::ifstream ifs(filePath, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buf.GetData()), buf.GetDataSize());
    ifs.close();
    return buf;
}

std::string FileIOManager::SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        m_Logger->warn("File dose not exist. {}", filePath.u8string());
        return std::string();
    }
    m_Logger->info("Open file: {}", filePath.u8string());
    std::ifstream ifs(filePath);
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

}  // namespace Hitagi::Core