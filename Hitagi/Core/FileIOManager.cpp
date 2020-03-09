#include <iostream>
#include <fstream>
#include "FileIOManager.hpp"

namespace Hitagi::Core {

int FileIOManager::Initialize() { return 0; }

void FileIOManager::Finalize() {}

void FileIOManager::Tick() {}

size_t FileIOManager::GetFileSize(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "[Warning-FileIOManager] File does not exists." << std::endl;
        std::cerr << "                    File path:" << filePath << std::endl;
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

Buffer FileIOManager::SyncOpenAndReadBinary(std::filesystem::path filePath) const {
    if (!checkFile(filePath)) return Buffer();

    Buffer        buf(std::filesystem::file_size(filePath));
    std::ifstream ifs(filePath, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buf.GetData()), buf.GetDataSize());
    ifs.close();
    return buf;
}

std::string FileIOManager::SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const {
    if (!checkFile(filePath)) return std::string();

    std::ifstream ifs(filePath);
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

bool FileIOManager::checkFile(const std::filesystem::path& filePath) const {
    std::cout << "[FileIOManager] File: " << filePath << std::flush;
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "\n[Error-FileIOManager] File does not exists." << std::endl;
        return false;
    }
#if defined(_DEBUG)
    std::cout << " File size: " << std::filesystem::file_size(filePath) << " bytes." << std::endl;
#endif  // DEBUG
    return true;
}
}  // namespace Hitagi::Core